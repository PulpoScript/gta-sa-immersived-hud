# Traspaso para la próxima sesión

Pega este archivo (o su enlace) al empezar una sesión nueva con el asistente.

## Contexto

- Plugin ASI (C++, plugin-sdk) para **GTA San Andreas PC 1.0 US**: HUD que aparece cuando hace falta y con elementos reubicados.
- Viene del mod CLEO [gta-sa-hud-mod](https://github.com/PulpoScript/gta-sa-hud-mod) (prototipo con Modos 1 y 2). Allí está la investigación: `docs/INVESTIGACION.md`, `docs/MODO2.md` y la sección "Sin publicar" del CHANGELOG (direcciones de posición verificadas).
- El asistente no puede compilar ni ejecutar el juego: el autor compila, prueba y cuenta el resultado.

## Cómo trabajar

- Español. Un paso cada vez, esperando la confirmación del autor.
- Respuestas cortas. Para cada prueba, una tabla con qué probar y qué esperar.
- El autor descarga el código con `git pull` en `E:\dev\gta-sa-immersived-hud`.
- **Commits en este repositorio: autor `TentaCode <128027722+PulpoScript@users.noreply.github.com>`, sin mencionar al asistente** (pedido del autor). Nada del asistente en commits, código ni documentación. Se sube directamente a `main`.
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
| `0.1.0-dev.2` a `dev.14` | Diagnóstico de direcciones y montaje compacto (posiciones, tamaños, contorno, borde de barras, estrellas, oxígeno) | ✅ Probado y aprobado |
| `0.1.0-dev.15` / `dev.16` | HUD dinámico (reglas del Modo 2), huecos que se cierran, tecla ".", compensación de salud máxima | ✅ Mundo libre. ⏳ Falta probar en misiones |

**La `0.1.0` todavía no está cerrada:** el autor quiere probar en misiones y otros momentos antes. Al cerrarla: versión `0.1.0` en el código, CHANGELOG y documentación.

## Pruebas pendientes antes de cerrar la 0.1.0

| Situación | Qué mirar |
|---|---|
| Misiones que ocultan el radar o el HUD (minijuegos: billar, baloncesto, baile, casino) | Que el plugin no muestre el radar cuando la misión lo oculta |
| Escenas de vídeo y pantalla panorámica | Nada raro al volver |
| Muerte y hospital, arresto | Barras, dinero y radar tras reaparecer |
| Cargar otra partida desde el menú | Todo visible 3 s |
| Subir la salud máxima (misiones de gimnasio / ambulancia) | Que nada se mueva |
| Bucear, conducir, pilotar | Oxígeno y radar |
| Tecla TAB a pie | Panel de estadísticas visible |

## Herramientas de la sesión

- Comprobación de sintaxis sin MSVC: clang 20 y mingw g++ contra plugin-sdk (`check.sh` en el directorio temporal de la sesión; reinstalar con `apt-get install -y g++-mingw-w64-i686 clang-20` y clonar plugin-sdk).
- Fuente clave: [ReSA](https://github.com/pedrokilo/ReSA) (desensamblado de `DrawPlayerInfo` y `DrawWanted` 1.0 US).

## Regla de documentación

Con cada versión `-dev` se actualizan `CHANGELOG.md` y la tabla de [VERSIONES.md](VERSIONES.md).

## Lecciones

- Compilar con **Compilar → Compilar solución** (`Ctrl + Mayús + B`), no con ▶ / F5: un `.asi` no se ejecuta solo.
- Si el juego está abierto, la copia del `.asi` falla.
- `CHud::SetHelpMessage(texto, true, false, false)` funciona. La fuente del juego no tiene acentos.
- Durante las pruebas, quitar `UI.cs` (mod CLEO) de `CLEO\`: también usa F11.

## Próximo paso

Recoger los resultados de las pruebas en misiones y corregir lo que falle. Después, cerrar la `0.1.0`.
