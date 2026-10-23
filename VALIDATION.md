# Rapport de validation — Vocal2MIDITune 0.1.0

Date : 2026-10-23

## Réussi

### DSP C++ autonome

Compilation avec GCC en C++17 puis exécution de `tests/DspTests.cpp` :

- conversions MIDI/fréquence ;
- centre Pitch Bend 8192 ;
- YIN à 50, 110, 220, 440, 880 et 1500 Hz ;
- silence ;
- quantification de gammes.

Résultat : **tous les tests passent**.

### Tracker MIDI avec JUCE 8.0.4

Compilation et exécution de `tests/MidiTrackerTests.cpp` avec les modules JUCE Core et Audio Basics :

- premier Note On ;
- aucun retrigger sur une note tenue ;
- remplacement Note Off/Note On ;
- release après silence ;
- bend non centré pendant une déviation puis retour à 8192 ;
- fermeture de note quand la sortie MIDI est désactivée ;
- stability time ;
- Panic.

Résultat : **tous les tests passent**.

### Vérification syntaxique JUCE

`PluginProcessor.cpp`, `MidiNoteTracker.cpp` et `PluginEditor.cpp` ont été vérifiés avec les en-têtes JUCE 8.0.4. Une ambiguïté de surcharge `MidiMessage::noteOn` trouvée pendant ces contrôles a été corrigée par un cast explicite vers `juce::uint8`.

### Détecteur JavaScript

Le Worklet a été exécuté avec des sinus connus : 110, 220, 440 et 880 Hz sont détectés avec une erreur relative inférieure à 0,1 % dans ce test.

### Navigateur Chromium

Test de la page servie en HTTP local :

- chargement de l’interface et de l’AudioWorklet ;
- démarrage de « Voix de test » ;
- exemple observé : 219 Hz, A3, Note On canal 1, confiance 100 % ;
- application du preset 808 Glide ;
- sortie Web MIDI simulée : 15 Note On, 15 Note Off, 333 Pitch Bend ;
- dernier Pitch Bend : 8192 ;
- aucune erreur JavaScript ;
- affichage mobile à 390 px sans débordement horizontal.

L’outil Playwright 1.54.1 a été utilisé, car la version 1.55 testée possède un problème connu de chargement d’AudioWorklet en mode headless.

## Non validé ici

- Aucun VST3 Windows x64 n’a été produit dans cet environnement Linux.
- La compilation JUCE graphique complète du bac à sable a été interrompue : JUCE `juceaide` dépasse la mémoire disponible de 2 Go pendant sa compilation. Cela ne constitue pas une erreur du code du plugin, mais ce n’est pas non plus une build Release réussie.
- Le scan dans Ableton, le routage avec un synthé réel et la restauration d’un Set Live restent à exécuter sous Windows.

Les commandes de build Windows et la liste des tests hôte restants figurent dans `README.md`.
