# SimpleVerb – reverb VST3 (JUCE)

Parametry: Room Size, Damping, Width, Pre-delay, Low Cut, High Cut, Mix, Freeze.

## Najprościej: gotowy plik przez GitHub (bez instalowania czegokolwiek)
1. Załóż repozytorium na github.com i wrzuć do niego całą zawartość tego folderu
   (razem z ukrytym folderem `.github`).
2. Wejdź w zakładkę **Actions** → uruchomi się workflow "Build VST3" (ok. 5–10 min).
3. Po zakończeniu otwórz przebieg i na dole w **Artifacts** pobierz paczkę
   dla swojego systemu (`SimpleVerb-Windows-VST3` itd.).

## Instalacja
Skopiuj folder/plik `SimpleVerb.vst3` do:
- Windows: `C:\Program Files\Common Files\VST3`
- macOS:   `/Library/Audio/Plug-Ins/VST3` (jeśli system blokuje: `xattr -cr SimpleVerb.vst3`)
- Linux:   `~/.vst3`

W REAPERZE: Options → Preferences → Plug-ins → VST → **Re-scan** (Clear cache/re-scan),
potem dodaj efekt przez FX → "VST3: SimpleVerb (MyCompany)".

## Budowanie lokalnie
Wymagane: CMake ≥ 3.22, kompilator C++17 (Visual Studio 2022 / Xcode / GCC).
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
Wynik: `build/SimpleVerb_artefacts/Release/VST3/`
(na Linuksie doinstaluj pakiety z `.github/workflows/build.yml`).
