# Windows — anticipation et procédure de test à la main

> Tenu par Claude, lu par Jean-François sur sa machine Windows (D6, D15). **Rien n'a été vérifié sous Windows** : ni build MSVC, ni MinGW, ni exécution. Ce fichier liste ce que la lecture du code permet d'anticiper (points W de `docs/port/00-analyse-initiale.md`, confirmés en lecture à l'Étape 0), l'impact des changements de la mission 1 et la procédure proposée. Publié ici depuis `docs/port/WINDOWS.md` à l'étape 1A.

## 1. État des points W (lecture seule)

| ID | Constat | Effet attendu sous Windows | Propriétaire |
|---|---|---|---|
| W1 | Bibliothèque partagée sans export (`CMakeLists.txt` ; ni `__declspec(dllexport)` ni `CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS`) | MSVC ne produit pas de `.lib` d'import pour `CPPActressMAS.dll` : les exemples liés à la cible partagée ne se lient pas, sauf liaison accidentelle à la statique (W7) | mission 5 (`WINDOWS_EXPORT_ALL_SYMBOLS` ou macro d'export) |
| W2 | `/LTCG` passé en option de **compilation** (`CMakeLists.txt`, `Examples/CMakeLists.txt`) | avertissement `D9002` (option ignorée), LTO inactive ; il faudrait `/GL` à la compilation et `/LTCG` à l'édition de liens, ou `INTERPROCEDURAL_OPTIMIZATION` | mission 5 |
| W3 | Paho et OpenSSL | **Résolu en 1C** : Paho C++ 1.4.0 et Paho C 1.3.13 sont téléchargés et construits par le projet (`FetchContent`, archives épinglées par SHA-256, statiques, avec SSL) derrière `CAM_TRANSPORT_MQTT` ; plus aucun `find_package(PahoMqttCpp)`. Il ne reste qu'OpenSSL à fournir (vcpkg `openssl`), et seulement pour le transport MQTT : le preset `core` (ou `-DCAM_TRANSPORT_MQTT=OFF`) construit le cœur, la façade `cam_transport` (loopback seul), les exemples et les tests sans aucune dépendance réseau. Paho C est du C : ses avertissements MSVC n'engagent pas le projet | ce fichier (vérification à la main) |
| W4 | `Externals/ThreadPool/ThreadPool.h:63-76` : `return_type` défini pour Windows, Linux, Apple seulement ; MinGW passe par `std::result_of_t`, retiré en C++20 | MSVC : OK ; llvm-mingw (libc++) : refus ; GCC MinGW : accepté | 2B/2C ou mission 5 |
| W5 | Compilation croisée MinGW-w64 (GCC 13) : vérification syntaxique passée dans l'analyse d'origine ; aucune construction POSIX dans le cœur | un build CMake MinGW en Release échouerait de toute façon (B13 : `if (WIN32)` applique `/O2 /W4 /LTCG` à GCC) | mission 5 |
| W6 | `/arch:AVX2` (`Examples/CMakeLists.txt`) | propre à x86/x64 ; refusé sur Windows ARM64 ; jamais appliqué avec un générateur multi-config (le bloc teste `CMAKE_BUILD_TYPE`) | mission 5 |
| W7 | `OUTPUT_NAME CPPActressMAS` commun à la statique et à la DLL | `.lib` statique et `.lib` d'import homonymes : liaison accidentelle à la statique, ou erreur « multiple rules » avec Ninja | mission 5 |
| B13 | `if (WIN32)` au lieu de `if (MSVC)` ; blocs Release conditionnés à `CMAKE_BUILD_TYPE` (vide avec Visual Studio) | avec le générateur Visual Studio, **aucune** option Release du projet n'est appliquée ; avec MinGW, options MSVC passées à GCC | mission 5 |
| B16 | `CMakeLists.txt` en CRLF, `.gitignore` mixte, pas de `.gitattributes` | un éditeur Windows peut réécrire les fins de ligne ; `core.autocrlf` à surveiller | mission 5 |
| C18 | `std::default_random_engine` = `mt19937` sous MSVC, `uniform_int_distribution` propre à la STL MSVC | ordres aléatoires différents de macOS et Linux à graine égale (vérifié macOS/Linux : `ADBEC…` contre `DEBAC…`) | 2C (générateur par environnement, Q10) |

## 2. Impact des changements de la mission 1 (étape 1A)

- **`CMakePresets.json`** : aucun générateur imposé ; sous Windows, le générateur par défaut est Visual Studio (multi-config) : `CMAKE_BUILD_TYPE` y est vide, donc les options Release du projet restent inactives (B13) et le preset `release` n'a pas d'effet ; `cmake --build --preset debug --config Debug` reste nécessaire. Les presets `asan`, `tsan`, `coverage` et `bench` passent des options clang/gcc (`-fsanitize=…`, `-O3`) : non applicables à MSVC, à ne pas utiliser sous Windows.
- **Tests** (`tests/`) : `cam_probes` et `cam_approve` n'ont pas de dépendance POSIX (`popen` → `_popen` sous Windows) ; les sondes de crash n'ont de verdict que sous un sanitizer (ASan de MSVC non essayé) ; `tests/support/tcp_probe.hpp` a une branche Winsock2 (`ws2_32.lib`) **non compilée ici**. Les tests réseau se passent avec le code 77 sans `CAM_MQTT_HOST`/`CAM_MQTT_PORT`.
- **B18** : `target_link_options(-flto)` est sous le `else()` de `if (WIN32)` : sans effet sous Windows.
- **`bench/`** : `std::atomic`, `steady_clock`, rien de spécifique à une plateforme.

## 2 bis. Impact des changements de l'étape 1C (couche transport)

- **Cœur sans réseau** : `CPPActressMAS/transport/` (types, enveloppe, codec `compact`, `Endpoint`, loopback) n'utilise que la bibliothèque standard ; `PahoWrapper.hpp` et cereal ont quitté le cœur (`Externals/cereal` reste pour `Examples/MAS/Reactive.cpp` et `Examples/Simples/SendingObjects.cpp`, qui l'incluent eux-mêmes).
- **Transport MQTT** (`CPPActressMAS/transport/mqtt/`, option `CAM_TRANSPORT_MQTT`) : Paho construit par le projet (W3) ; `set_target_properties(paho-mqttpp3-static PROPERTIES CXX_STANDARD 20)` aligne Paho C++ sur le projet (B19 : indispensable aussi sous MSVC pour éviter un mélange de standards dans une même image) ; Paho C a besoin d'OpenSSL (`find_package(OpenSSL)`) : `vcpkg install openssl`. La première configuration télécharge deux archives GitHub (≈ 3,7 Mo) ; `-DFETCHCONTENT_SOURCE_DIR_PAHO_MQTT_CPP=<dossier>` et `-DFETCHCONTENT_SOURCE_DIR_PAHO_MQTT_C=<dossier>` permettent de pointer des copies locales hors ligne.
- **Threads** : `MqttTransport` possède un thread de travail (`std::thread`, `std::condition_variable`) et reçoit les rappels Paho sur les threads de Paho ; rien de POSIX. `Environment::move` dépose sa demande sous `std::mutex`.
- **Tests** : `integration.loopback` et `integration.migration` tournent sans réseau ni broker (déterministes, un tour par `start()`) ; `mqtt.network` saute (code 77) sans `CAM_MQTT_HOST`/`CAM_MQTT_PORT`, sans transport MQTT compilé, ou si le port ne répond pas ; le cas « reconnexion » appelle `std::system` sur le script `CAM_MQTT_BROKER_CONTROL` (`stop`, `start`) : sous Windows, fournir un `.cmd` équivalent ou laisser la variable vide (cas ignoré, les quatre autres tournent).
- **Exemples réseau** : `Mobile [hôte [port]]` et `MobilePingPong [hôte [port]]` (défaut `127.0.0.1 1883`) via `cam::transport::connect` ; le patch d'arguments d'`outils/mqtt/` n'est plus nécessaire.
- **Vecteurs binaires** (`tests/vectors/envelope/compact/v1/*.bin`) : petit-boutiste par construction du codec, indépendants de la plateforme ; `git` ne doit pas les convertir (pas de `.gitattributes` encore, B16 : vérifier `core.autocrlf` n'altère pas les `.bin`, qui ne contiennent pas de `\r\n` significatif mais pourraient en contenir par hasard).

## 2 ter. Impact de l'étape 2A (transport Zenoh)

- **Transport Zenoh** (`CPPActressMAS/transport/zenoh/`, option `CAM_TRANSPORT_ZENOH`, OFF par défaut) : zenoh-c et zenoh-cpp **1.7.1 exactement** (`find_package(zenohc 1.7.1 EXACT)` puis `find_package(zenohcxx 1.7.1 EXACT)`). Sous Windows : archives GitHub `zenoh-c-1.7.1-x86_64-pc-windows-msvc-standalone.zip` et `zenoh-cpp-1.7.1-standalone.zip`, décompressées dans un préfixe, puis `-Dzenohc_DIR=<préfixe>\lib\cmake\zenohc -Dzenohcxx_DIR=<préfixe>\lib\cmake\zenohcxx`. Le `CMakeLists.txt` lie `zenohc::static` sur macOS (`libzenohc.a`) et `zenohc::shared` partout ailleurs quand le paquet fournit cette cible (Linux : `libzenohc.so`, lisible par ThreadSanitizer comme un module à part, T10) : sous Windows ce sera donc la dynamique, `zenohc.dll` à côté de l'exécutable (ou dans le `PATH`) ; si l'on force la statique, `zenohc.lib` exige les bibliothèques système du runtime Rust (`ws2_32`, `userenv`, `bcrypt`, `ntdll`, `advapi32`). Les en-têtes de zenoh-cpp exigent C++17 au moins et `ZENOHCXX_ZENOHC` (posé par le projet).
- **Tests** : `zenoh.network` ouvre des sockets TCP sur `127.0.0.1` (ports 17447 à 17451), aucun multicast ; le pare-feu Windows peut demander une autorisation à la première exécution. `ctest -C Debug -LE zenoh` pour l'éviter.
- **Exemples** : `Mobile` et `MobilePingPong` sans argument sont des pairs Zenoh locaux ; `Mobile mqtt://127.0.0.1:1883/mobile` pour MQTT.

## 3. Procédure de build proposée (à exécuter par toi, non vérifiée)

Prérequis : Visual Studio 2022 (MSVC 19.3x, C++20), CMake ≥ 3.22, vcpkg (OpenSSL seulement), accès réseau à GitHub à la première configuration avec `CAM_TRANSPORT_MQTT=ON`.

```powershell
# Cœur seul, sans aucune dépendance (recommandé en premier)
cmake -S . -B build\msvc-core -DBUILD_EXAMPLES=ON -DCAM_TRANSPORT_MQTT=OFF
cmake --build build\msvc-core --config Debug
ctest --test-dir build\msvc-core -C Debug --output-on-failure

# Avec le transport MQTT : OpenSSL par vcpkg (triplet dynamique par défaut ; pour la statique : x64-windows-static)
vcpkg install openssl --triplet x64-windows
cmake -S . -B build\msvc -DBUILD_EXAMPLES=ON -DCAM_TRANSPORT_MQTT=ON ^
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build\msvc --config Debug
ctest --test-dir build\msvc -C Debug --output-on-failure
```

Attendu après 1C : la configuration du cœur passe sans vcpkg ; l'édition de liens des exemples et des tests peut encore échouer sur la `.lib` d'import absente (W1) ; avec MQTT, Paho C peut produire des avertissements MSVC (code C, hors projet). Note les messages exacts ; ils serviront en mission 5.

## 4. Procédure de test à la main
1. Exemples hors ligne : `Exemple1` à `Exemple5`, `MultipleMessages`, `SendingObjects`, `NumberGame`, `ColorGame`, `Serialization`, `Reactive`, `Skynet` (attendu `Sum 49995000`). `PingPong` : non borné (P7), à éviter.
2. Tests : `ctest -C Debug -L unit`, `-L integration` (approbation : sorties normalisées, mêmes attendus que macOS en principe) ; `-L characterization` sans sanitizer : seules les sondes « ok » et les défauts connus à valeur ont un verdict.
3. Sondes : attendu C18 : la sonde d'ordre séquentiel aléatoire donne une chaîne différente de `ADBECADECBDCEABCBEADDEBAC` (libc++) et `DEBACAEDBCEDBCAAEDBCCBDEA` (libstdc++) : note-la.
4. Réseau : Mosquitto Windows ou le broker de `cam` exposé ; `CAM_MQTT_HOST`/`CAM_MQTT_PORT` puis `ctest -C Debug -L mqtt` (quatre cas ; le cinquième attend `CAM_MQTT_BROKER_CONTROL`) ; `Mobile <hôte> <port>` et `MobilePingPong <hôte> <port>` prennent l'hôte et le port en arguments. Attendu pour `Mobile` : le mobile visite les deux autres environnements, collecte six « Info from agent … » et imprime son rapport ; pour `MobilePingPong` : la requête puis « Info 1 » et « Info 2 ».

## 5. Journal des tests Windows
_(vide : aucun test effectué)_
