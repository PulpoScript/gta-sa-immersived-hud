# GTA SA Immersived HUD

Plugin ASI para **GTA San Andreas PC 1.0 US** que hace el HUD más inmersivo: cada elemento aparece solo cuando hace falta y se puede colocar en otra posición.

> **Estado:** en desarrollo. Todavía no hay ninguna versión publicada.

## Qué hará

- Mostrar cada elemento (hora, dinero, barras de salud y blindaje, estrellas, icono de arma) solo cuando pasa algo, y ocultarlo al rato.
- Colocar esos elementos en otra posición, sin que se solapen.
- Mostrar todo el HUD manteniendo una tecla.

## Origen

Continúa las ideas probadas en [gta-sa-hud-mod](https://github.com/PulpoScript/gta-sa-hud-mod), un mod CLEO que sirvió de prototipo (Modos 1 y 2). Este proyecto es independiente: **no uses los dos a la vez**.

## Requisitos

- GTA San Andreas PC **1.0 US**.
- Un cargador de ASI ([Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) o [modloader](https://github.com/thelink2012/modloader)).

## Compilar

Necesitas Visual Studio con el compilador **MSVC v143** y [plugin-sdk](https://github.com/DK22Pac/plugin-sdk) compilado (`Plugin_SA`, Release), con las variables de entorno `PLUGIN_SDK_DIR` y `GTA_SA_DIR`.

1. Abre `ImmersivedHud.sln`.
2. Configuración **Release | Win32** → **Compilar → Compilar solución**.
3. El resultado, `bin\Release\ImmersivedHUD.asi`, se copia solo a `GTA_SA_DIR`. Si el juego está abierto, la copia falla: ciérralo y vuelve a compilar.

## Controles (versión actual)

| Tecla | Acción |
|---|---|
| . (punto) | Toque corto: la hora 5 s. Mantener 0,8 s: todo el HUD visible con la nueva distribución; al soltar, se oculta a los 3 s |
| F11 | Apaga / enciende el plugin (empieza encendido). Apagado: HUD de fábrica |
| F10 | Guarda `ImmersivedHud.log` (diagnóstico, solo lectura) |

## Compatibilidad

- [Widescreen Fix](https://github.com/ThirteenAG/WidescreenFixesPack) (ThirteenAG) también modifica la posición del icono de arma y la munición. Este plugin tendrá que convivir con él.

## Créditos

- [plugin-sdk](https://github.com/DK22Pac/plugin-sdk) (DK22Pac y colaboradores): clases y direcciones del juego.
- [gta-reversed](https://github.com/gta-reversed/gta-reversed): posiciones de fábrica del HUD y funciones `CHud::Draw`, `DrawBarChart` y `GetYPosBasedOnHealth`.
- [ReSA](https://github.com/pedrokilo/ReSA) (pedrokilo): desensamblado de `CHud::DrawPlayerInfo` y `CHud::DrawWanted` (1.0 US), con el desplazamiento por salud máxima de cada elemento.
- [Widescreen Fix](https://github.com/ThirteenAG/WidescreenFixesPack) (ThirteenAG): técnica de redirigir el puntero de cada instrucción a un float propio.
- Silent (GTAForums): truco de ocultar el dinero quitando la llamada a `CFont::PrintString`.

## Aviso legal

Proyecto de fans, no afiliado a Rockstar Games ni a Take-Two Interactive. GTA San Andreas es una marca de sus respectivos propietarios. Necesitas una copia legal del juego.

## Licencia

[MIT](LICENSE)
