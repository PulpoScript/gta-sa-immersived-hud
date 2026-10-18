# Registro de cambios

Formato basado en [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/). Aún no hay releases publicadas.

## [Sin publicar]

### 0.1.0-dev.10 — Contorno más fino en la hora y el dinero (sin probar)

- Con F11, el contorno de la hora y el dinero pasa de 2 a 1 (`CFont::SetEdge`, entero: no hay valores intermedios). Se busca `push 2; call 0x719590` en el código de cada uno y solo se cambia si aparece una vez.
- F10 indica dónde está ese contorno y busca en `CSprite2d::DrawBarChart` (`0x728640`) el grosor del borde de las barras (2 unidades, compartido con todas las barras del juego). Las barras no se cambian todavía.

### 0.1.0-dev.9 — Dinero más pequeño y grupo más compacto (probado)

- ✅ El autor da por bueno el montaje completo. Pide un contorno negro un poco más fino en hora, dinero y barras.

- Dinero con la misma letra que la hora (`0x58F54E` alto, `0x58F564` ancho: 0,64 / 0,32).
- Dinero, blindaje, salud y estrellas suben para cerrar el espacio liberado; la salud termina a la altura del icono.

### 0.1.0-dev.8 — Icono en la esquina y hora más pequeña (probado)

- ✅ Icono pegado a la esquina; hora con letra al 58 %: el autor da ese tamaño por bueno.
- Encontrados: letra del dinero (`0x58F54E` = 1,1, `0x58F564` = 0,55, mismas constantes que la hora); tamaño del icono 47 × 58 (`0x58D8CB`/`0x58D935` y `0x58D896`/`0x58D94D`).
- Salud y blindaje tienen una segunda pasada (`0x58EED0` = 194, `0x58EEFD` = 141, `0x58EFA1` = 164, `0x58EFCE` = 94): probablemente el segundo jugador. No se toca.
- Estrellas: tamaño 1,21 / 0,605 en `0x58DCAA`/`0x58DCC0`, `0x58DD70`/`0x58DD86` y `0x58DF5D`/`0x58DF79`.

- El grupo se pega a la esquina: icono, munición, hora, dinero, barras y estrellas se desplazan unas 20 unidades a la derecha; icono y hora suben.
- Hora con letra al 58 % (`0x58EB31` alto, `0x58EB47` ancho): prueba de cambio de tamaño.
- F10 busca también en el código del dinero, las barras y el dibujo del icono (punteros de tamaño).
- Objetivo (boceto del autor): texto al ~60 % del tamaño original, barras más largas y finas, estrellas más pequeñas.

### 0.1.0-dev.7 — Montaje completo: munición, estrellas y márgenes (probado)

- ✅ La munición sigue al icono; las estrellas apagadas se alinean con las encendidas; los márgenes son correctos.
- Pendiente: proporciones. En el boceto el texto es más pequeño y el grupo está pegado a la esquina.

- La munición sigue al icono (`0x58FA02`, el "32" de su X).
- Las estrellas apagadas usan la misma Y que las encendidas (`0x58DEF5`, `0x58DFB3`).
- Más separación entre hora, dinero y blindaje; estrellas debajo de la salud.

### 0.1.0-dev.6 — Búsqueda de la posición de la munición (probado)

- Munición: X = ancho − (32 [`0x58FA02`] + parte de WidescreenFix) + 47 × 0,5; Y = 20 [`0x58F9DC`] + 43 [`0x58F9E6`].
- Icono Y: `0x58F913` (→ `0x858BA4` = 20).
- Estrellas: además de `0x58DDFC`, la Y 114 se usa en `0x58DEF5` y `0x58DFB3`. Tamaño 1,21 / 0,605.
- `0x58FA4C`–`0x58FAB1` repite el cálculo de la munición con un desplazamiento de 138: probablemente el segundo jugador. No se toca.
- La escala de WidescreenFix depende de la resolución (a 1920 × 1080, 1 unidad ≈ 1,93 px).

- F10 también busca instrucciones de float entre `0x58F880` y `0x58FAC0` (icono y munición) y entre `0x58DC80` y `0x58E020` (estrellas).

### 0.1.0-dev.5 — Montaje completo fijo (probado en parte)

- ✅ Icono, hora, dinero y salud quedan donde se esperaba. Dinero muy pegado a la hora.
- ❌ La munición no sigue al icono: se queda en su sitio de fábrica, sobre el hueco del blindaje.
- Estrellas: `0x58DDFC` mueve solo las estrellas encendidas; las apagadas usan otra Y (sin encontrar). La X está bien.
- Blindaje: bien colocado, pero dinero, hora y blindaje demasiado juntos.

- Con F11 encendido, el HUD pasa al montaje compacto: icono en la esquina; a su izquierda hora, dinero, blindaje y salud; estrellas debajo. Al apagar, todo vuelve a su sitio.
- Valores de primera aproximación, sacados de las capturas. Todavía no es dinámico.

### 0.1.0-dev.4 — Búsqueda de la posición de la hora (probado)

- Encontrada: Hora Y en `0x58EC04` (→ `0x858F8C` = 22) y Hora X en `0x58EC16` (→ `0x85950C` = 32, compartido con Dinero X y Arma X). `0x58EB31` (1,1) y `0x58EB47` (0,55) son el tamaño de letra.
- F10 también anota en `ImmersivedHud.log` cada instrucción de float (`fadd`, `fmul`, `fsub`, `fsubr`, `fdiv`, `fdivr`, `fld`) entre `0x58EA80` y `0x58EC80`, con su puntero y su valor. Solo lectura.

### 0.1.0-dev.3 — Primer movimiento: el dinero (probado)

- Probado: el dinero se mueve al encender, vuelve a su sitio al apagar y el resto del HUD no cambia.
- Con WidescreenFix a 1920 × 1080, 1 unidad del HUD ≈ 1,93 píxeles en ambos ejes.
- La Y del dinero es su borde de arriba, menos 12 si la salud máxima es menor de 101. La X del blindaje es su borde izquierdo y la Y su borde de arriba.

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
