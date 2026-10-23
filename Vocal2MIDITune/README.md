# Vocal2MIDITune 0.1.0

Plugin **voix monophonique → MIDI** pour Windows et Ableton Live, avec projet JUCE 8 / CMake et prototype interactif dans le navigateur.

Le détail des contrôles exécutés et des validations restantes est consigné dans [`VALIDATION.md`](VALIDATION.md).

- Entrée/sortie audio mono ou stéréo ; audio traversant.
- Détection YIN avec autocorrélation FFT, interpolation et confiance.
- Note On/Off, une note active, stabilité et release.
- Vélocité dynamique ou fixe, courbes Linear/Soft/Hard.
- Scale Lock, 13 gammes, transposition et octave.
- Modes Mono/Legato, Glide et Pitch Bend MIDI 14 bits.
- Interface de monitoring, six presets et sauvegarde APVTS.
- Application Standalone en plus du VST3.

> **Important :** cette V1 ne modifie pas la hauteur de l’audio. Elle génère des notes et du Pitch Bend MIDI. Le paramètre Dry/Wet est réservé à une future correction audio et n’a donc volontairement aucun effet dans cette version.

---

## 1. Tester immédiatement sans Ableton

Le dossier `web-demo/` contient une version interactive avec :

- une **mélodie vocale synthétique intégrée** ;
- une entrée **microphone** ;
- une entrée **fichier audio** ;
- un synthé d’écoute interne ;
- une sortie **Web MIDI** optionnelle ;
- les réglages de détection, gamme, MIDI, glide et pitch bend.

### Dans la prévisualisation fournie

Clique sur **Démarrer le test**. La source « Voix de test » ne demande aucun microphone. Le synthé d’écoute est activé par défaut.

Essaie ensuite :

1. **Natural Vocal MIDI** pour le comportement initial ;
2. **Hard Tune MIDI** pour des notes fortement quantifiées sans bend ;
3. **808 Glide** pour entendre les transitions Always/Smooth ;
4. **Expressive Synth Lead** pour le vibrato et le bend.

« Entrée audible » est coupé par défaut afin de distinguer le synthé MIDI de la source. Avec un microphone, utilise un casque avant de l’activer.

### En local sur Windows

Un petit serveur HTTP est nécessaire pour charger l’AudioWorklet. Depuis le dossier `web-demo` :

```bat
cd C:\Projects\Vocal2MIDITune\web-demo
py -m http.server 8000
```

Puis ouvre :

```text
http://localhost:8000
```

Chrome ou Edge récent est recommandé. Le microphone exige une autorisation. Web MIDI peut être indisponible dans une iframe de prévisualisation ; dans ce cas, le synthé interne reste utilisable. Pour relier la démo à Ableton sous Windows, un port virtuel tel que loopMIDI est nécessaire.

Le prototype partage les mêmes algorithmes musicaux, mais **ce n’est pas le VST3** et il ne remplace pas les tests dans Live.

---

## 2. Prérequis pour compiler le VST3 Windows

- Windows 10 ou 11, 64 bits.
- Visual Studio Community 2022.
- Workload **Desktop development with C++**.
- MSVC v143 x64/x86, Windows 10/11 SDK et C++ CMake tools.
- CMake 3.22 ou plus récent.
- Git si JUCE est téléchargé automatiquement.
- Ableton Live Suite pour le test d’intégration.

Deux options pour JUCE :

1. Laisser CMake télécharger **JUCE 8.0.4** avec FetchContent ; ou
2. Installer JUCE dans `C:\JUCE` et passer `-DJUCE_PATH="C:/JUCE"`.

JUCE possède sa propre licence ; voir `LICENSE` pour le code de ce projet.

---

## 3. Compilation Windows

Dans un terminal « x64 Native Tools Command Prompt for VS 2022 » ou PowerShell correctement configuré :

```bat
cd C:\Projects\Vocal2MIDITune
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 -DJUCE_PATH="C:/JUCE"
cmake --build build --config Debug
cmake --build build --config Release
```

Sans installation locale de JUCE :

```bat
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Tests algorithmiques :

```bat
ctest --test-dir build -C Release --output-on-failure
```

Copie automatique optionnelle après compilation :

```bat
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 -DVOCAL2MIDI_COPY_PLUGIN_AFTER_BUILD=ON
```

Selon les droits du système, la copie automatique peut nécessiter un terminal administrateur. La copie manuelle ci-dessous est plus prévisible.

### Artefacts attendus

```text
C:\Projects\Vocal2MIDITune\build\Vocal2MIDITune_artefacts\Release\VST3\Vocal2MIDITune.vst3
C:\Projects\Vocal2MIDITune\build\Vocal2MIDITune_artefacts\Release\Standalone\Vocal2MIDITune.exe
```

Sous Windows, `.vst3` est un **dossier bundle**, même si l’Explorateur peut le présenter comme un élément unique.

### Installation manuelle

Copie le dossier complet :

```text
Vocal2MIDITune.vst3
```

vers :

```text
C:\Program Files\Common Files\VST3\
```

Ne copie pas uniquement le DLL qu’il contient.

---

## 4. Configuration dans Ableton Live

1. Ouvre `Preferences → Plug-ins`.
2. Active **Use VST3 Plug-In System** selon la langue et la version de Live.
3. Rescane les plug-ins et recherche **Vocal2MIDITune**.
4. Place le plugin sur la piste audio du microphone ou de la voix enregistrée.
5. Laisse l’audio de cette piste audible : le plugin est traversant, hormis Output Gain.
6. Crée une piste MIDI avec Wavetable, Operator, Drift ou une 808.
7. Sur la piste instrument, choisis la piste voix dans **MIDI From**. Si Live propose un second menu, sélectionne la sortie MIDI de **Vocal2MIDITune**.
8. Règle le monitoring de la piste instrument sur **In**, ou arme-la selon ton routage.
9. Chante ou lis une voix monophonique et vérifie les Note On/Off dans le plugin et le synthé.
10. Règle le **Pitch Bend Range du synthé cible exactement comme celui du plugin** : ±2, ±3, ±5, ±12 ou ±24 demi-tons.

Pour un premier test, utilise 128 ou 256 échantillons de buffer et le preset **Natural Vocal MIDI**. Une fenêtre YIN d’environ 80 ms à 48 kHz, le hop et le Stability Time introduisent une latence de détection MIDI ; le chemin audio reste traversant sans délai de correction.

---

## 5. Structure

```text
Vocal2MIDITune/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── Source/
│   ├── PluginProcessor.h/.cpp
│   ├── PluginEditor.h/.cpp
│   ├── PitchDetector.h/.cpp
│   ├── ScaleQuantizer.h/.cpp
│   ├── MidiNoteTracker.h/.cpp
│   ├── MidiUtils.h
│   ├── ParameterIDs.h
│   └── LookAndFeel.h
├── assets/presets/
│   ├── 01-natural-vocal-midi.json
│   ├── 02-hard-tune-midi.json
│   ├── 03-808-glide.json
│   ├── 04-expressive-synth-lead.json
│   ├── 05-piano-notes.json
│   └── 06-bass-vocal.json
├── tests/DspTests.cpp
└── web-demo/
    ├── index.html
    ├── styles.css
    ├── app.js
    └── pitch-worklet.js
```

Les six presets sont appliquables depuis l’interface du plugin et de la démo. Les fichiers JSON documentent les mêmes réglages et peuvent servir à un futur gestionnaire de presets utilisateur.

---

## 6. Validation effectuée

Les tests C++ `tests/DspTests.cpp` vérifient :

- A4 = 440 Hz = MIDI 69 ;
- conversion fréquence/MIDI et centre Pitch Bend 8192 ;
- détection de sinus à 50, 110, 220, 440, 880 et 1500 Hz ;
- silence sans note détectée ;
- quantification chromatique, majeure et mineure.

Le test JUCE `tests/MidiTrackerTests.cpp` vérifie aussi le premier Note On, l’absence de retrigger, le remplacement de note, le Note Off après silence, le retour du bend à 8192, la stabilité, la désactivation de la sortie et Panic.

Le même détecteur JavaScript est vérifié à 110, 220, 440 et 880 Hz. Les buffers DSP C++ sont alloués dans `prepare()` ; `processSample()` et `analyse()` ne créent pas de vecteurs. Les sources du processor, du tracker et de l’éditeur ont également été vérifiés syntaxiquement avec les en-têtes JUCE 8.0.4.

### Tests encore nécessaires sur la machine Windows/Ableton

- génération effective du bundle VST3 Windows x64 avec MSVC ;
- scan et chargement dans la version exacte de Live utilisée ;
- routage MIDI avec le synthé choisi ;
- Note Off et retour du bend au centre après plusieurs silences/transitions ;
- sauvegarde/réouverture d’un Set Live et restauration des paramètres ;
- performance avec de vraies voix, prises de parole, vibrato et réverbération.

Aucun binaire Windows n’est affirmé comme compilé tant que ces commandes n’ont pas abouti sur la machine Windows cible.

---

## 7. Limites connues de la V1

- Monophonique uniquement ; accords, chœurs et sources très réverbérées peuvent produire des erreurs.
- Pas de correction Auto-Tune audio, de pitch-shifting, de formants ou d’harmonies.
- Dry/Wet est réservé et sans effet ; Output Gain est fonctionnel.
- Le Glide est représenté par Pitch Bend et dépend du comportement du synthé cible.
- Une transition supérieure à la plage de bend est limitée pour éviter des valeurs MIDI invalides.
- Le synthé doit utiliser la même plage de Pitch Bend que le plugin.
- Le Standalone sert surtout au test audio/UI ; le routage MIDI dépend des possibilités du périphérique et de l’application hôte.
- La démo Web MIDI n’est pas un driver VST3 et peut nécessiter loopMIDI sous Windows.

Une V2 « Auto-Tune audio » devra ajouter un moteur de pitch-shifting temps réel séparé, avec Retune Speed, Humanize, préservation des formants et du vibrato. Elle ne doit pas être présentée comme équivalente à Antares Auto-Tune sans une étape DSP et des tests supplémentaires.
