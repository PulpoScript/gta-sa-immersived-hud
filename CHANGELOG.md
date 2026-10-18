# Registro de cambios

Formato basado en [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/). Aún no hay releases publicadas.

## [Sin publicar]

### 0.1.0-dev.3 — Primer movimiento: el dinero (sin probar)

- Con F11 encendido, el dinero pasa a X = 100, Y = 140 (posición de prueba); al apagar vuelve a la de fábrica.
- Técnica: se redirige el puntero de la instrucción (`0x58F5FC`, `0x58F5DC`) a un float propio. Solo se escribe si el puntero es el de fábrica.

### 0.1.0-dev.2 — Diagnóstico de direcciones (probado)

Resultado en el juego (con WidescreenFix):

| Elemento | Dirección | Puntero de fábrica | Valor |
|---|---|---|---|
| Dinero X / Y | `0x58F5FC` / `0x58F5DC` | `0x85950C` / `0x866C88` | 32 / 89 |
| Salud X / Y | `0x58EE87` / `0x58EE68` | `0x86535C` / `0x866CA8` | 141 / 77 |
| Blindaje X / Y | `0x58EF59` / `0x58EF3A` | `0x866B78` / `0x862D38` | 94 / 48 |
| Oxígeno X | `0x58F11F` | `0x866B78` (compartido con Blindaje X) | 94 |
| Estrellas X / Y | `0x58DD0F` / `0x58DDFC` (`fld`) | `0x863210` / `0x866C5C` | 29 / 114 |
| Arma X (a) | `0x58F927` | `0x85950C` (compartido con Dinero X) | 32 |
| `0x58F9DC` (`fld`) | | `0x858BA4` | 20 (probablemente Y del icono) |

- Todas son `fmul [mem]` (`D8 0D`) salvo las marcadas `fld` (`D9 05`).
- WidescreenFix redirige Arma X (b), Munición X y las escalas de hora y dinero: escala 1/800 × 1/560 en vez de 1/640 × 1/448.
- La posición de la hora sigue sin encontrarse.


- F10 escribe `ImmersivedHud.log` junto a `gta_sa.exe` con los bytes, el puntero y el float de cada dirección candidata a posición del HUD.
- Solo lee la memoria del juego; no cambia nada.
- Siguiente objetivo acordado: montaje compacto arriba a la derecha (icono en la esquina; a su izquierda hora, dinero, blindaje y salud; estrellas debajo) en el que los elementos ocultos dejan su hueco y el resto se recoloca.

### 0.1.0-dev.1 — Proyecto mínimo (probado)

- Proyecto de Visual Studio basado en plugin-sdk (`Plugin_SA`, Release).
- F11 enciende y apaga el plugin y lo anuncia con `CHud::SetHelpMessage`.
- Al compilar, el `.asi` se copia a `GTA_SA_DIR`.
