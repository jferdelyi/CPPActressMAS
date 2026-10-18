# Windows — anticipation et procédure de test à la main

> Tenu par Claude, lu par Jean-François sur sa machine Windows (D6, D15). **Rien n'a été vérifié sous Windows** : ni build MSVC, ni MinGW, ni exécution. Ce fichier liste ce que la lecture du code permet d'anticiper (points W de `docs/port/00-analyse-initiale.md`, confirmés en lecture à l'Étape 0), l'impact des changements de la mission 1 et la procédure proposée. Publié ici depuis `docs/port/WINDOWS.md` à l'étape 1A.

## 1. État des points W (lecture seule)

| ID | Constat | Effet attendu sous Windows | Propriétaire |
|---|---|---|---|
| W1 | Bibliothèque partagée sans export (`CMakeLists.txt` ; ni `__declspec(dllexport)` ni `CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS`) | MSVC ne produit pas de `.lib` d'import pour `CPPActressMAS.dll` : les exemples liés à la cible partagée ne se lient pas, sauf liaison accidentelle à la statique (W7) | mission 5 (`WINDOWS_EXPORT_ALL_SYMBOLS` ou macro d'export) |
| W2 | `/LTCG` passé en option de **compilation** (`CMakeLists.txt`, `Examples/CMakeLists.txt`) | avertissement `D9002` (option ignorée), LTO inactive ; il faudrait `/GL` à la compilation et `/LTCG` à l'édition de liens, ou `INTERPROCEDURAL_OPTIMIZATION` | mission 5 |
| W3 | libmosquitto et OpenSSL | **Résolu en 1C avec Paho, remplacé en 2A-bis par libmosquitto 2.0.22** : téléchargée et construite par le projet (`FetchContent`, archive épinglée par SHA-256, statique, threads, TLS) derrière `CAM_TRANSPORT_MQTT`. Il reste OpenSSL à fournir (vcpkg `openssl`) et, sous Windows, **pthreads4w** (vcpkg `pthreads`) : libmosquitto cherche `Pthreads4W` quand `WITH_THREADING` est actif, et le projet l'exige (les verrous de la bibliothèque permettent de publier depuis le thread de simulation). Le preset `core` (ou `-DCAM_TRANSPORT_MQTT=OFF`) construit le cœur, la façade `cam_transport` (loopback seul), les exemples et les tests sans aucune dépendance réseau. libmosquitto est du C : ses avertissements MSVC n'engagent pas le projet | ce fichier (vérification à la main) |
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

- **Depuis 2C** : une seule bibliothèque (`CPPActressMAS` partagée, `CPPActressMASStatic` statique) ; les transports activés par leurs options y sont compilés avec leurs dépendances, il n'y a plus de `cam_transport`, `cam_transport_mqtt` ni `cam_transport_zenoh` à lier (`cam_transport` reste un alias). Une application se connecte par `environment.connect(url)`.

- **Cœur sans réseau** : `CPPActressMAS/transport/` (types, enveloppe, codec `compact`, `Endpoint`, loopback) n'utilise que la bibliothèque standard ; `PahoWrapper.hpp` et cereal ont quitté le cœur (`Externals/cereal` reste pour `Examples/MAS/Reactive.cpp` et `Examples/Simples/SendingObjects.cpp`, qui l'incluent eux-mêmes).
- **Transport MQTT** (`CPPActressMAS/transport/mqtt/`, option `CAM_TRANSPORT_MQTT`) : libmosquitto construite par le projet (W3), bibliothèque cliente seule (`WITH_BROKER=OFF`, `WITH_CLIENTS=OFF`, `WITH_APPS=OFF`, `WITH_PLUGINS=OFF`, `WITH_CJSON=OFF`, `DOCUMENTATION=OFF`, `WITH_LIB_CPP=OFF`), statique, `WITH_THREADING=ON` (pthreads4w sous Windows : `vcpkg install pthreads`), TLS par OpenSSL (`find_package(OpenSSL)` : `vcpkg install openssl`), `ws2_32` lié par libmosquitto. La première configuration télécharge une archive GitHub (≈ 2,2 Mo) ; `-DFETCHCONTENT_SOURCE_DIR_LIBMOSQUITTO=<dossier>` pointe une copie locale hors ligne.
- **Threads** : `MqttTransport` possède le thread réseau de libmosquitto (`std::thread` qui exécute `mosquitto_loop_forever`, `std::condition_variable` pour les acquittements) ; les rappels arrivent sur ce thread ; rien de POSIX dans le projet (libmosquitto elle-même utilise pthreads4w sous Windows). `Environment::move` dépose sa demande sous `std::mutex`.
- **Tests** : `integration.loopback` et `integration.migration` tournent sans réseau ni broker (déterministes, un tour par `start()`) ; `mqtt.network` saute (code 77) sans `CAM_MQTT_HOST`/`CAM_MQTT_PORT`, sans transport MQTT compilé, ou si le port ne répond pas ; le cas « reconnexion » appelle `std::system` sur le script `CAM_MQTT_BROKER_CONTROL` (`stop`, `start`) : sous Windows, fournir un `.cmd` équivalent ou laisser la variable vide (cas ignoré, les quatre autres tournent).
- **Exemples réseau** : `Mobile [hôte [port]]` et `MobilePingPong [hôte [port]]` (défaut `127.0.0.1 1883`) via `cam::transport::connect` ; le patch d'arguments d'`outils/mqtt/` n'est plus nécessaire.
- **Vecteurs binaires** (`tests/vectors/envelope/compact/v1/*.bin`) : petit-boutiste par construction du codec, indépendants de la plateforme ; `git` ne doit pas les convertir (pas de `.gitattributes` encore, B16 : vérifier `core.autocrlf` n'altère pas les `.bin`, qui ne contiennent pas de `\r\n` significatif mais pourraient en contenir par hasard).

## 2 ter. Impact de l'étape 2A (transport Zenoh)

- **Transport Zenoh** (`CPPActressMAS/transport/zenoh/`, option `CAM_TRANSPORT_ZENOH`, OFF par défaut) : zenoh-c et zenoh-cpp **1.7.1 exactement** (`find_package(zenohc 1.7.1 EXACT)` puis `find_package(zenohcxx 1.7.1 EXACT)`). Sous Windows : archives GitHub `zenoh-c-1.7.1-x86_64-pc-windows-msvc-standalone.zip` et `zenoh-cpp-1.7.1-standalone.zip`, décompressées dans un préfixe, puis `-Dzenohc_DIR=<préfixe>\lib\cmake\zenohc -Dzenohcxx_DIR=<préfixe>\lib\cmake\zenohcxx`. Le `CMakeLists.txt` lie `zenohc::static` sur macOS (`libzenohc.a`) et `zenohc::shared` partout ailleurs quand le paquet fournit cette cible (Linux : `libzenohc.so`, lisible par ThreadSanitizer comme un module à part, T10) : sous Windows ce sera donc la dynamique, `zenohc.dll` à côté de l'exécutable (ou dans le `PATH`) ; si l'on force la statique, `zenohc.lib` exige les bibliothèques système du runtime Rust (`ws2_32`, `userenv`, `bcrypt`, `ntdll`, `advapi32`). Les en-têtes de zenoh-cpp exigent C++17 au moins et `ZENOHCXX_ZENOHC` (posé par le projet).
- **Codecs d'enveloppe** (`transport/codec/{CompactCodec,ProtobufCodec,JsonCodec}.cpp`) : code portable sans dépendance (le codec `protobuf` écrit le format fil proto3 lui-même, pas de libprotobuf ni de protoc à installer) ; `envelope.proto` n'est pas compilé par le projet.
- **Tests** : `zenoh.network` ouvre des sockets TCP sur `127.0.0.1` (ports 17447 à 17454), aucun multicast ; le pare-feu Windows peut demander une autorisation à la première exécution. `ctest -C Debug -LE zenoh` pour l'éviter.
- **Exemples** : `Mobile` et `MobilePingPong` sans argument démarrent sur `loopback://` (en mémoire, depuis D46) ; une URL Zenoh ou MQTT en argument (`Mobile mqtt://127.0.0.1:1883/mobile`).

## 2 quater. Impact de l'étape 3B avancée (performance du cœur)

- **Identifiants** (`CPPActressMAS/util/Identifier.{h,cpp}`) : `std::random_device`, `std::mt19937_64` et `thread_local`, portables (MSVC les fournit). Plus d'en-tête `Externals/uuid`.
- **Mode parallèle** (`CPPActressMAS/util/TurnPool.{h,cpp}`) :
  - `std::atomic::wait` et `notify_*` (C++20) : MSVC 19.28 et suivants, Windows 8 et suivants (`WaitOnAddress`) ;
  - attente active par `_mm_pause` sur x64 (`<immintrin.h>`). Sous ARM64 avec MSVC, aucune instruction de pause n'est émise : l'attente est seulement plus coûteuse en énergie ;
  - `Externals/ThreadPool` n'est plus utilisé.
- **Non vérifié sous Windows** (D6).

## 3. Procédure de build proposée (à exécuter par toi, non vérifiée)

Prérequis : Visual Studio 2022 (MSVC 19.3x, C++20), CMake ≥ 3.22, vcpkg (OpenSSL et pthreads pour le transport MQTT), accès réseau à GitHub à la première configuration avec `CAM_TRANSPORT_MQTT=ON`.

```powershell
# Cœur seul, sans aucune dépendance (recommandé en premier)
cmake -S . -B build\msvc-core -DBUILD_EXAMPLES=ON -DCAM_TRANSPORT_MQTT=OFF
cmake --build build\msvc-core --config Debug
ctest --test-dir build\msvc-core -C Debug --output-on-failure

# Avec le transport MQTT : OpenSSL et pthreads4w par vcpkg (triplet dynamique par défaut ; pour la statique : x64-windows-static)
vcpkg install openssl pthreads --triplet x64-windows
cmake -S . -B build\msvc -DBUILD_EXAMPLES=ON -DCAM_TRANSPORT_MQTT=ON ^
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build\msvc --config Debug
ctest --test-dir build\msvc -C Debug --output-on-failure
```

Attendu après 1C : la configuration du cœur passe sans vcpkg ; l'édition de liens des exemples et des tests peut encore échouer sur la `.lib` d'import absente (W1) ; avec MQTT, libmosquitto peut produire des avertissements MSVC (code C, hors projet) et exige `Pthreads4W` (W3). Note les messages exacts ; ils serviront en mission 5.

## 4. Procédure de test à la main
1. Exemples hors ligne : `Exemple1` à `Exemple5`, `MultipleMessages`, `SendingObjects`, `NumberGame`, `ColorGame`, `Serialization`, `Reactive`, `Skynet` (attendu `Sum 49995000`). `PingPong` : non borné (P7), à éviter.
2. Tests : `ctest -C Debug -L unit`, `-L integration` (approbation : sorties normalisées, mêmes attendus que macOS en principe) ; `-L characterization` sans sanitizer : seules les sondes « ok » et les défauts connus à valeur ont un verdict.
3. Sondes : attendu C18 : la sonde d'ordre séquentiel aléatoire donne une chaîne différente de `ADBECADECBDCEABCBEADDEBAC` (libc++) et `DEBACAEDBCEDBCAAEDBCCBDEA` (libstdc++) : note-la.
4. Réseau : Mosquitto Windows ou le broker de `cam` exposé ; `CAM_MQTT_HOST`/`CAM_MQTT_PORT` puis `ctest -C Debug -L mqtt` (quatre cas ; le cinquième attend `CAM_MQTT_BROKER_CONTROL`) ; `Mobile <hôte> <port>` et `MobilePingPong <hôte> <port>` prennent l'hôte et le port en arguments. Attendu pour `Mobile` : le mobile visite les deux autres environnements, collecte six « Info from agent … » et imprime son rapport ; pour `MobilePingPong` : la requête puis « Info 1 » et « Info 2 ».

## 5. Journal des tests Windows
_(vide : aucun test effectué)_
