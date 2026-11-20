# Compiler CyMCPHub / Build CyMCPHub

Git, Python 3 pour les tests et CMake 3.24 ou plus récent sont nécessaires. CMake récupère les versions épinglées de CyUI, CyMCPSDK et des autres bibliothèques. Il applique au SDK le correctif de compatibilité Streamable HTTP du projet.

### Windows

Installez Visual Studio 2022 avec les outils C++ et le SDK Windows.

```powershell
cmake -S . -B build -G 'Visual Studio 17 2022' -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure --no-tests=error
cmake --install build --config Release --component Hub --prefix stage
```

Le runtime Microsoft Visual C++ x64 est requis pour exécuter la version Release.

### Linux

Commandes pour Ubuntu 24.04 x64. SDL3 fournit le backend graphique.

```bash
sudo apt-get install build-essential ninja-build libcurl4-openssl-dev libx11-dev libxext-dev libxrandr-dev \
  libxcursor-dev libxi-dev libxfixes-dev libxss-dev libxtst-dev libwayland-dev wayland-protocols libxkbcommon-dev \
  libegl1-mesa-dev libgl1-mesa-dev libdbus-1-dev libdecor-0-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCURL_NO_CURL_CMAKE=ON -DCURL_LIBRARY=/usr/lib/x86_64-linux-gnu/libcurl.so \
  -DCURL_INCLUDE_DIR=/usr/include/x86_64-linux-gnu
cmake --build build --parallel
ctest --test-dir build --output-on-failure --no-tests=error
cmake --install build --component Hub --prefix stage
```

### macOS

Utilisez Xcode ou ses outils en ligne de commande, avec une version de libc++ qui prend en charge `std::jthread`. La configuration ci-dessous produit une application universelle.

```bash
sdk="$(xcrun --sdk macosx --show-sdk-path)"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DCURL_NO_CURL_CMAKE=ON "-DCURL_LIBRARY=$sdk/usr/lib/libcurl.tbd" \
  "-DCURL_INCLUDE_DIR=$sdk/usr/include"
cmake --build build --parallel
ctest --test-dir build --output-on-failure --no-tests=error
cmake --install build --component Hub --prefix stage
```

Le workflow manuel [Native packages](https://github.com/MrMybal/CyMCPHub/blob/main/.github/workflows/native-packages.yml) construit et teste les trois paquets sur leurs systèmes respectifs. Le paquet macOS de ce workflow utilise une signature locale ad hoc.

## Manuel / Manual

Avec Node.js 22+, régénérez le lecteur après avoir modifié les sources de `.CyDocBuild`.

Use Node.js 22+ and regenerate the reader after editing the `.CyDocBuild` sources.

```sh
npm ci --prefix .CyDocBuild
npm --prefix .CyDocBuild run check
```

Ouvrez `Documentation/Documentation.html` dans votre navigateur. Le paquet généré par `cmake --install` inclut le lecteur hors ligne complet.

Open `Documentation/Documentation.html` in a browser. The package produced by `cmake --install` includes the complete offline reader.

## Archiver un paquet local / Archive a local package

Les commandes ci-dessous archivent le dossier `stage` généré par CMake. / These commands archive the `stage` directory produced by CMake.

**Windows**

```powershell
Compress-Archive -Path stage/* -DestinationPath CyMCPHub-windows-x64.zip
```

**Linux**

```bash
tar -C stage -czf CyMCPHub-linux-x64.tar.gz .
```

**macOS**

```bash
ditto -c -k --sequesterRsrc stage CyMCPHub-macos-universal.zip
```
