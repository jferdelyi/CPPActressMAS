# Windows — anticipation et procédure de test à la main

> Tenu par Claude, lu par Jean-François sur sa machine Windows (D6, D15). **Rien n'a été vérifié sous Windows** : ni build MSVC, ni MinGW, ni exécution. Ce fichier liste ce que la lecture du code permet d'anticiper (points W de `docs/port/00-analyse-initiale.md`, confirmés en lecture à l'Étape 0), l'impact des changements de la mission 1 et la procédure proposée. Publié ici depuis `docs/port/WINDOWS.md` à l'étape 1A.

## 1. État des points W (lecture seule)

| ID | Constat | Effet attendu sous Windows | Propriétaire |
|---|---|---|---|
| W1 | Bibliothèque partagée sans export (`CMakeLists.txt` ; ni `__declspec(dllexport)` ni `CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS`) | MSVC ne produit pas de `.lib` d'import pour `CPPActressMAS.dll` : les exemples liés à la cible partagée ne se lient pas, sauf liaison accidentelle à la statique (W7) | mission 5 (`WINDOWS_EXPORT_ALL_SYMBOLS` ou macro d'export) |
| W2 | `/LTCG` passé en option de **compilation** (`CMakeLists.txt`, `Examples/CMakeLists.txt`) | avertissement `D9002` (option ignorée), LTO inactive ; il faudrait `/GL` à la compilation et `/LTCG` à l'édition de liens, ou `INTERPROCEDURAL_OPTIMIZATION` | mission 5 |
| W3 | Paho et OpenSSL : via vcpkg ; nom de cible Paho selon le triplet (`x64-windows` dynamique : `PahoMqttCpp::paho-mqttpp3` seulement, pas `-static`) | **1A (B1)** : le `CMakeLists.txt` choisit la cible disponible (`paho-mqttpp3-static` si elle existe, sinon `paho-mqttpp3`) : la configuration devrait passer avec le triplet par défaut ; à vérifier. Ligne de liaison statique connue dans le dépôt `paho_client` (`CMakeLists.txt:14-17`) | 1C (noms de cibles), ce fichier |
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

## 3. Procédure de build proposée (à exécuter par toi, non vérifiée)

Prérequis : Visual Studio 2022 (MSVC 19.3x, C++20), CMake ≥ 3.22, vcpkg.

```powershell
# Dépendances (triplet dynamique par défaut ; pour la statique : x64-windows-static)
vcpkg install paho-mqttpp3 openssl --triplet x64-windows

# Configuration (générateur multi-config : les options Release du projet ne s'appliquent pas, B13)
cmake -S . -B build\msvc -DBUILD_EXAMPLES=ON ^
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build\msvc --config Debug
ctest --test-dir build\msvc -C Debug --output-on-failure
```

Attendu après 1A : la configuration passe (B1 choisit `PahoMqttCpp::paho-mqttpp3`) ; l'édition de liens des exemples et des tests peut échouer sur la `.lib` d'import absente (W1). Note les messages exacts ; ils serviront en 1C et en mission 5.

## 4. Procédure de test à la main
1. Exemples hors ligne : `Exemple1` à `Exemple5`, `MultipleMessages`, `SendingObjects`, `NumberGame`, `ColorGame`, `Serialization`, `Reactive`, `Skynet` (attendu `Sum 49995000`). `PingPong` : non borné (P7), à éviter.
2. Tests : `ctest -C Debug -L unit`, `-L integration` (approbation : sorties normalisées, mêmes attendus que macOS en principe) ; `-L characterization` sans sanitizer : seules les sondes « ok » et les défauts connus à valeur ont un verdict.
3. Sondes : attendu C18 : la sonde d'ordre séquentiel aléatoire donne une chaîne différente de `ADBECADECBDCEABCBEADDEBAC` (libc++) et `DEBACAEDBCEDBCAAEDBCCBDEA` (libstdc++) : note-la.
4. Réseau : Mosquitto Windows ou le broker de `cam` exposé ; `CAM_MQTT_HOST`/`CAM_MQTT_PORT` puis `ctest -C Debug -L mqtt` ; `Mobile` et `MobilePingPong` ont besoin du patch d'arguments (`docs/port/outils/mqtt/exemples-reseau-args.patch`) pour viser un autre hôte/port.

## 5. Journal des tests Windows
_(vide : aucun test effectué)_
