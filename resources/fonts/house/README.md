# House fonts

The fonts of the Reckless build (`KICAD_HOUSE_FONTS`). A build without that option installs none of them.

| Font | Files | License |
|---|---|---|
| Noto Serif, the interface and the schematic text default | `NotoSerif-Regular.ttf`, `NotoSerif-Bold.ttf` | SIL Open Font License 1.1, `OFL-NotoSerif.txt` |
| Monaspace Xenon, the overlay and fixed-width font | `MonaspaceXenon-Regular.otf`, `MonaspaceXenon-Bold.otf` | SIL Open Font License 1.1, `OFL-MonaspaceXenon.txt` |

Source: `assets/fonts/` of the public `reckless-technology/reckless-toolkit` repository, copied unchanged from
commit `4755c80338c19470cadf663cc3c06f8df115f67d` (the fonts last changed in `8435338aabd39247c9e08527077d19dad7c4a633`).
The OFL texts are the toolkit's `assets/fonts/OFL.txt` and `assets/fonts/monaspace/OFL.txt`.

They install to `<KiCad data directory>/fonts/house/` (`share/kicad/fonts/house` on Linux and Windows,
`Contents/SharedSupport/fonts/house` in the macOS bundle), and at startup KiCad adds that directory to its own
fontconfig, so `Fontconfig()->FindFont( "Noto Serif" )` and `FindFont( "Monaspace Xenon" )` succeed with nothing
installed on the system.
