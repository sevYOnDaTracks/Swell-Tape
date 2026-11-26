# Swell Tape

Bande magnétique en 2-en-1 : **bande studio pour le mix** et **cassette lo-fi**, reliées par un seul potard : **Age**.

VST3 · AU · Standalone. macOS universel (Apple Silicon + Intel) et Windows (VST3 + Standalone, via GitHub Actions). JUCE 8.

## Réglages

| Réglage | Rôle |
|---|---|
| Drive | pousse la bande : saturation douce, compression, harmoniques (les aigus saturent en premier, comme sur une vraie bande) |
| Wow | variations de vitesse : wow lent, flutter rapide et dérive aléatoire. Discret en studio, très marqué en cassette |
| **Age** | Studio 30 ips → Vintage 15 ips → Vieille bande 7,5 ips → Cassette fatiguée. Fait glisser ensemble la perte d'aigus, la bosse de tête, le souffle, l'asymétrie de la saturation, l'ampleur du wow et les drops |
| Tone | inclinaison du spectre : plus brillant ou plus sombre (±10) |
| Mix | mélange avec le son sec, à phase alignée : le sec est retardé de la même latence que la bande, donc aucun effet de filtre en peigne |
| Plus → Souffle | Coupé, Auto (suit Age) ou Fort |
| Plus → Vitesse | Suit Age, ou fige le profil de vitesse (30 / 15 / 7,5 ips, cassette) pendant qu'Age ne garde que l'usure |
| Plus → Sortie | niveau de sortie (±12 dB) |
| Plus → Arrêt de bande | la bande ralentit jusqu'à l'arrêt (≈ 1 s) ; un second clic la relance (démarrage ≈ 0,4 s) |

8 presets d'usine, de « Colle de bus » à « Cassette oubliée ».

## Le son

- **Enregistrement** : pré-accentuation des aigus → saturation tanh asymétrique avec anti-repliement ADAA (pas de suréchantillonnage, pas de latence en plus) → désaccentuation → souffle.
- **La bande** : tampon circulaire lu à vitesse variable, avec interpolation Hermite. C'est ce qui donne le wow, le flutter et l'arrêt ou le démarrage de bande.
- **Lecture** : coupe-bas et bosse de tête, de plus en plus graves quand la bande ralentit (110 Hz → 42 Hz) ; passe-bas de 22 kHz (30 ips) à ≈ 5 kHz (cassette) ; tonalité ; drops aléatoires sur une bande usée.
- Latence annoncée à l'hôte : 4 ms, le centre de la modulation de vitesse.

## Compiler

```bash
scripts/build_mac.sh
scripts/install_mac.sh
```

Banc de test (contrôles audio chiffrés, WAV à écouter, captures de l'interface) :

```bash
cmake -B build-test -DSWELL_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-test --target SwellTapeTest
build-test/SwellTapeTest_artefacts/Release/SwellTapeTest test-out [fichier.wav]
```

## Licence

Système de licences Swell commun (`Source/Licence`), produit `swell-tape` sur swell-audio.fr.

## Crédits

Photo des bobines et de la platine : « Studer B67 reel-to-reel audio tape recorder, ca. 1978 », Robert Taylor (Stirling, ON, Canada), [CC BY 2.0](https://creativecommons.org/licenses/by/2.0), via Wikimedia Commons. Découpée et retouchée (marques retirées, fenêtres détourées). Voir `Assets/CREDITS.txt`.
