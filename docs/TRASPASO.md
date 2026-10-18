# Traspaso para la próxima sesión

Pega este archivo (o su enlace) al empezar una sesión nueva con Claude.

## Contexto

- Plugin ASI (C++, plugin-sdk) para **GTA San Andreas PC 1.0 US**: HUD que aparece cuando hace falta y con elementos reubicados.
- Viene del mod CLEO [gta-sa-hud-mod](https://github.com/PulpoScript/gta-sa-hud-mod) (prototipo con Modos 1 y 2). Allí está la investigación: `docs/INVESTIGACION.md`, `docs/MODO2.md` y la sección "Sin publicar" del CHANGELOG (direcciones de posición verificadas).
- Claude no puede compilar ni ejecutar el juego: el autor compila, prueba y cuenta el resultado.

## Cómo trabajar

- Español. Un paso cada vez, esperando la confirmación del autor.
- Respuestas cortas. Para cada prueba, una tabla con qué probar y qué esperar.
- El autor descarga el código con `git pull` en `E:\dev\gta-sa-immersived-hud`.
- **Commits en este repositorio: autor `TentaCode <128027722+PulpoScript@users.noreply.github.com>`, sin mencionar a Claude** (pedido del autor). Se sube directamente a `main`.
- No afirmar direcciones ni funciones sin verificarlas. Dar crédito a cada fuente en el README.

## Entorno del autor (verificado)

| Qué | Dónde |
|---|---|
| Visual Studio Community 2026 + MSVC v143 | — |
| plugin-sdk | `E:\dev\plugin-sdk` (`PLUGIN_SDK_DIR`), librería `output\lib\Plugin.lib` |
| Juego | `E:\GTA_SA_1.0\GTA_SA_1.0` (`GTA_SA_DIR`); los `.asi` van en la raíz |
| Mods que tocan la pantalla | `GTASA.WidescreenFix.asi` (redirige arma X `0x58F92F` y munición X `0x58F9F7`), SilentPatchSA, skygfx, WindowedMode, MobileSubtitles |

## Estado

| Versión | Contenido | Estado |
|---|---|---|
| `0.1.0-dev.1` | Proyecto mínimo: F11 activa / desactiva con mensaje | ✅ Probado |

## Lecciones

- Compilar con **Compilar → Compilar solución** (`Ctrl + Mayús + B`), no con ▶ / F5: un `.asi` no se ejecuta solo.
- Si el juego está abierto, la copia del `.asi` falla.
- `CHud::SetHelpMessage(texto, true, false, false)` funciona. La fuente del juego no tiene acentos.
- Durante las pruebas, quitar `UI.cs` (mod CLEO) de `CLEO\`: también usa F11.

## Próximo paso

Definir con el autor el primer comportamiento: probablemente ocultar y mostrar un elemento (como el Modo 2 del mod CLEO) o mover uno (el dinero es el más sencillo: punteros de posición verificados en `0x58F5FC` y `0x58F5DC`).
