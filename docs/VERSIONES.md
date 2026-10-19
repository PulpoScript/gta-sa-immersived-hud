# Esquema de versiones

El proyecto usa [versionado semántico](https://semver.org/lang/es/): `MAYOR.MENOR.PARCHE`.

| Parte | Cuándo sube | Ejemplo |
|---|---|---|
| **MAYOR** | `0` mientras el plugin está en beta. Pasa a `1` en la primera release pública, con todo probado. | `0.x.x` → `1.0.0` |
| **MENOR** | Cada vez que se completa una función grande (HUD dinámico, forma del radar…). | `0.1.0` = HUD dinámico completo |
| **PARCHE** | Correcciones y mejoras pequeñas. | `0.1.1` = corregir un fallo en misiones |

## Versiones en desarrollo

Mientras una función está a medias, la versión lleva un sufijo con el número de prueba (`0.1.0-dev.1`, `0.1.0-dev.2`…). Las versiones `-dev` no se publican como release: son estados intermedios en GitHub.

## Plan y estado

| Versión | Contenido | Estado |
|---|---|---|
| `0.1.0-dev.1` | Proyecto mínimo: F11 enciende y apaga el plugin | ✅ Probado |
| `0.1.0-dev.2` | Diagnóstico con F10: lectura de las direcciones candidatas | ✅ Probado |
| `0.1.0-dev.3` | Primer movimiento: el dinero (técnica de redirigir punteros) | ✅ Probado |
| `0.1.0-dev.4` | Búsqueda de la posición de la hora | ✅ Probado |
| `0.1.0-dev.5` | Montaje compacto fijo, primera aproximación | ✅ Probado en parte |
| `0.1.0-dev.6` | Búsqueda de la munición y las estrellas | ✅ Probado |
| `0.1.0-dev.7` | Munición con el icono, estrellas alineadas, márgenes | ✅ Probado |
| `0.1.0-dev.8` | Icono en la esquina, hora más pequeña | ✅ Probado |
| `0.1.0-dev.9` | Dinero con la letra de la hora, grupo compacto | ✅ Aprobado |
| `0.1.0-dev.10` | Contorno más fino en hora y dinero | ✅ Probado |
| `0.1.0-dev.11` | Volcado de `DrawBarChart` para el borde de las barras | ✅ Probado |
| `0.1.0-dev.12` | Barras más pequeñas (solo se aplicó el alto de la salud) | ✅ Aprobado |
| `0.1.0-dev.13` | Borde de las barras más fino, estrellas más pequeñas | ✅ Aprobado |
| `0.1.0-dev.14` | Oxígeno abajo a la derecha | ✅ Probado |
| `0.1.0-dev.15` | HUD dinámico (reglas del Modo 2), huecos que se cierran, compensación de salud máxima | ✅ Probado (se cambió la tecla ".") |
| `0.1.0-dev.16` | Mantener "." muestra todo con la nueva distribución | ✅ Mundo libre · ⏳ falta probar en misiones |
| `0.1.0-dev.17` | Diferencia de dinero debajo del dinero (+ verde / − rojo) | ⏳ Sin probar |
| `0.1.0-dev.18` | Radar visible con objetivo de misión o destino propio | 🔁 Probada: el radar tardaba 8 s en ocultarse al quitar el destino |
| `0.1.0-dev.19` | Menú de depuración (F9) con ajustes guardados en `.ini`; radar sin espera al quitar el destino | ⏳ Sin probar |
| `0.1.0-dev.20` | Barra de resistencia (donde el oxígeno) | ⏳ Sin probar |
| `0.1.0-dev.21` | CJ quieto con el menú F9 abierto (opción por defecto) | ✅ Probado (el menú se salía de la pantalla) |
| `0.1.0-dev.22` | El menú F9 cabe en pantalla y se desplaza | ⏳ Sin probar |
| `0.1.0` | HUD dinámico completo | ⏳ Tras las pruebas en misiones |
| `0.2.0-dev.1` | Radar rectangular, paso 1: volcado del código del radar con F10 | ✅ Probado |
| `0.2.0-dev.2` | Forma del radar en el menú F9: redondo, cuadrado, 3:2 y 16:9 | 🔁 Probado: la "N" sigue en círculo y el marco la tapa |
| `0.2.0-dev.3` | Marco bajo los iconos; F10 identifica los mods que enganchan el radar | ✅ Probado (la "N" sigue en círculo) |
| `0.2.0-dev.4` | Diagnóstico de la "N": volcado de `DrawBlips` y llamadas a `LimitRadarPoint` | ✅ Probado |
| `0.2.0-dev.5` | La "N" sigue el borde del rectángulo | ✅ Probado |
| `0.2.0-dev.6` | Iconos de corto alcance visibles en todo el rectángulo | ⏳ Sin probar |
| `0.2.0-dev.7` | Opacidad del HUD (global o por elemento) y menú F9 con subpáginas | 🔁 Probado: faltaban el radar y el color del dinero |
| `0.2.0-dev.8` | Opacidad del radar y del color del dinero | ⏳ Sin probar |
| `0.2.0` | Forma del radar elegible en el menú F9 (redondo / rectangular) | ⏳ |
| `1.0.0` | Primera release pública | ⏳ |

El detalle de cada versión (direcciones, valores y resultados de las pruebas) está en el [CHANGELOG](../CHANGELOG.md).

## Dónde se indica la versión

Al cambiar de versión hay que actualizar **los tres sitios a la vez**:

1. Cabecera de `src/main.cpp`: línea `Versión: ...`.
2. Constante `VERSION` en `src/main.cpp`: es la que muestran los mensajes de F11 en el juego.
3. `CHANGELOG.md` y la tabla de este archivo.

Cuando una versión se publique como release, se creará también una etiqueta de git con el mismo nombre (por ejemplo, `v1.0.0`).
