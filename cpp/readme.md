# Как развернуть тестовый проект

1. [Установите CMake](https://cmake.org/install/) версии не меньше 3.20
    * Windows: скачайте [один из установщиков](https://cmake.org/download/) под Windows (например, [версию 4.4.3 для x64](https://github.com/Kitware/CMake/releases/download/v4.4.3/cmake-4.4.3-windows-x86_64.msi)) и установите (не забудьте выбрать пункт `Add CMake to current user PATH` при установке, иначе CMake не удастся запустить из терминала)
    * Linux: поставьте пакет CMake из официальных репозиториев (`apt install cmake`, etc)
    * mac OS:
        * Скачайте [один из образов](https://cmake.org/download/) под macOS (например, [версию 4.4.3 для macOS 10.10](https://github.com/Kitware/CMake/releases/download/v4.4.3/cmake-4.4.3-macos10.10-universal.dmg)) и установите
        * Либо поставьте CMake через [Homebrew](https://brew.sh/): `brew install cmake`
2. Установите [библиотеку SDL3](https://www.libsdl.org/)
    * Windows: скачайте **dev-архив** (например, [версию 3.4.14 под Visual Studio](https://github.com/libsdl-org/SDL/releases/download/release-3.4.14/SDL3-devel-3.4.14-VC.zip)) и распакуйте куда-нибудь
    * Linux: поставьте **dev-пакет** SDL3 из официальных репозиториев (`apt install libsdl3-dev`, etc)
    * mac OS:
        * Скачайте **dev-образ** под macOS (например, [версию 3.4.14](https://github.com/libsdl-org/SDL/releases/download/release-3.4.14/SDL3-3.4.14.dmg)) и установите
        * Либо поставьте SDL3 через [Homebrew](https://brew.sh/): `brew install sdl3`
3. Установите библиотеку [wgpu-native](https://github.com/gfx-rs/wgpu-native) версии [v.29.0.1.1](https://github.com/gfx-rs/wgpu-native/releases/tag/v29.0.1.1)
    * Windows: скачайте [сборку под x86_64 Visual Studio](https://github.com/gfx-rs/wgpu-native/releases/download/v29.0.1.1/wgpu-windows-x86_64-msvc-release.zip)
    * Linux: скачайте [сборку под x86_64 Linux](https://github.com/gfx-rs/wgpu-native/releases/download/v29.0.1.1/wgpu-linux-x86_64-release.zip)
    * mac OS:
        * Скачайте [сборку под x86_64 macOS](https://github.com/gfx-rs/wgpu-native/releases/download/v29.0.1.1/wgpu-macos-x86_64-release.zip) или [сборку под aarch64 macOS](https://github.com/gfx-rs/wgpu-native/releases/download/v29.0.1.1/wgpu-macos-aarch64-release.zip), в зависимости от вашего процессора
        * Либо поставьте wgpu-native через [Homebrew](https://brew.sh/): `brew version-install wgpu-native@29.0.1.1`
4. Склонируйте куда-нибудь [репозиторий с кодом для практического занятия](https://github.com/lisyarus/realtime-graphics-course-v2-practice.git):
    `git clone https://github.com/lisyarus/realtime-graphics-course-v2-practice.git`
5. Соберите тестовый проект `cpp/practice01`:
    * Windows:
        1. Создайте где-нибудь директорию сборки, например `<путь-до-репозитория>/cpp/build`
        2. Откройте терминал (стандартный `cmd`, или лучше PowerShell, Git Bash, Cygwin, etc) и перейдите в директорию сборки `<путь-до-репозитория>/cpp/build`
        3. Сконфигурируйте сборку, запустив CMake: 
           `cmake .. -G '<генератор>' -DWGPU_NATIVE_ROOT='<путь до библиотеки wgpu-native>' -DCMAKE_INSTALL_PREFIX='<путь до библиотеки SDL3>`, где
            * `<генератор>` - выбранный вами [CMake-генератор](https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html), например `'Visual Studio 17 2022'`, если вы хотите сконфигурировать проект для работы с Visual Studio 17 2022 (полный список Visual Studio генераторов есть [здесь](https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html#visual-studio-generators))
            * `'<путь до библиотеки wgpu-native>'` - путь до директории, куда вы распаковали библиотеку `wgpu-native` (в ней должны быть директории `lib`, `include`, и `wgpu-native-meta`)
            * `'<путь до библиотеки SDL3>'` - путь до директории, куда вы распаковали библиотеку SDL3 (в ней должны быть директории `cmake`, `include`, `lib` и несколько `.txt` и `.md`-файлов)
            _При использовании нестандартного эмулятора терминала могут возникнуть проблемы с форматом путей, но CMake должен нормально распознать стандартные Windows-пути (`C:\Users\Me\Projects\...`)_
            _В некоторых терминалах нужно использовать двойные, а не одинарные кавычки_
        4. Соберите проект: `cmake --build .`
        5. Скопируйте необходимые библиотеки (`.../lib/x64/SDL3.dll` и `.../lib/wgpu_native.dll`) в директорию с собранным проектом (т.е. рядом с исполняемым файлом `practice01`) и погорюйте о том, что в Windows нет концепции [RPath](https://en.wikipedia.org/wiki/Rpath). К сожалению, этот этап придётся повторять для каждой практики.
        6. Запустите исполняемый файл `practice01`, должно появиться пустое окно (возможно, чёрного или розового цвета)
            * Пункты `d` и `f` можно выполнить из Visual Studio, если вы использовали соответствующий генератор, -- для этого нужно открыть Visual Studio solution, сгенерированный в директории сборки, и выбрать practice01 (*вместо ALL_BUILD!*) в качестве стартового проекта
    * Windows с помощью CMake-GUI:
        1. В директории с проектом создайте директорию сборки `<путь-до-репозитория>/cpp/build`
        2. Запустите CMake-GUI (он идёт в стандартном коплекте с CMake, т.е. скорее всего уже у вас установлен)
        3. В качестве директории с исходным кодом (`Where is the source code:`) укажите путь до директории `<путь-до-репозитория>/cpp`
        4. В качестве директории сборки (`Where to build the binaries:`) укажите путь до директории сборки (`<путь-до-репозитория>/cpp/build`)
        5. Добавьте переменную `WGPU_NATIVE_ROOT` с путём до библиотеки `wgpu-native`:
            1. Нажмите `Add Entry`
            2. Введите имя `WGPU_NATIVE_ROOT`
            3. Выберите тип `PATH`
            4. Выберите соответствующий путь в (`Value`)
        6. Аналогично, добавьте переменную `CMAKE_PREFIX_PATH` с путём до библиотеки SDL3
        7. Нажмите `Configure`
        8. Нажмите `Generate`
        9. Дальнейшие действия совпадают с пунктами `d-f` в описании сборки под Windows
    * Linux/macOS:
        1. Убедитесь, что вы находитесь в директории с проектом `<путь-до-репозитория>/cpp`
        2. Создайте директорию сборки и перейдите в неё: `mkdir build && cd build`
        3. Сконфигурируйте сборку, запустив CMake: `cmake .. -DWGPU_NATIVE_ROOT='<путь до библиотеки wgpu-native>'`
        4. Соберите проект: `cmake --build .`
        5. Запустите собранный проект, должно появиться пустое окно (возможно, чёрного или розового цвета): `./practice01/practice01`
    * Любая ОС, CLion:
        1. На старте выберите "открыть проект", и откройте CMakeLists.txt в корне тестового проекта (`<путь-до-репозитория>/cpp/CMakeLists.txt`) *как проектный файл* (CLion может спросить об этом)
        2. Под windows: нужно создать CMake-профиль с путями до библиотек:
            1. Откройте `File > Settings > Build, Execution, Deployment > CMake`
            2. Если не хотите менять текущий профиль (по умолчанию - Debug), создайте новый (плюсик сверху списка профилей)
            3. В строке `CMake options` введите `-DWGPU_NATIVE_ROOT='<путь до библиотеки wgpu-native>' -DCMAKE_PREFIX_PATH='<путь до библиотеки SDL3>` (подробнее об этих путях см. в пункте `Windows.c` выше)
                * Генератор при этом указывать не нужно!
            4. Закройте окно настроек
            5. Если вы создавали новый профиль, выберите его в списке конфигураций (справа вверху)
        3. Запустите проект (Run / `[F6]`), должно появиться пустое окно (возможно, чёрного или розового цвета)
