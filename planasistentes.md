# Proyecto: Asistentes de IA Multi-Personalidad en M5Stack Tab5

**Versión:** 1.0 · **Documento maestro de planeación**

## 1. Visión

Una tablet M5Stack Tab5 sobre el escritorio que convive conmigo en español. Le hablo por palabra de activación, y 2–3 asistentes con personalidades, voces y tareas distintas responden: uno para conversación filosófica, otro que detecta mi estrés y me acompaña, otro para temas de IA. Toda la configuración editable desde un panel web. Costo fijo: $0.

**Criterio de éxito del proyecto completo:** le digo "Aura..." al aire, ella responde con su voz, me escucha sin botones, recuerda la conversación, y puedo cambiarle la personalidad desde mi celular.

## 2. Arquitectura

```
 Tab5 (ESP32-P4, ESP-IDF)      Cloudflare Worker           Proveedores ($0)
──────────────────────        ─────────────────────       ─────────────────
 mic → WAV ──────────────────→ /chat
                                  ├─ valida token
 "Aura!" (wake word)             ├─ carga personalidad (KV)
 pantalla: avatar + texto        ├─ STT + respuesta + emoción ──→ Gemini 2.5 Flash
 parlante ←── audio ←─────────┤   ├─ historial de contexto (D1)     (audio-in directo)
                                  └─ texto → voz ────────────────→ Groq PlayAI TTS
 /admin (panel web) ←────────── config editable desde PC/celular
```

| Componente | Elección | Costo | Justificación |
|---|---|---|---|
| STT + LLM + emoción | Gemini 2.5 Flash (audio-in) | $0 | Una sola llamada hace las tres cosas |
| TTS | Groq PlayAI | $0 | Sin tarjeta, voces ES distintas, devuelve WAV PCM directo a I2S |
| Servidor | Cloudflare Worker + KV + D1 | $0 | Orquestación, config y persistencia gratis |
| Wake word | Picovoice Porcupine (uso personal) | $0 | Español, keywords personalizadas |
| Plan B LLM | Groq (Whisper + Llama) / Flash-Lite | $0 | Si golpeo límites de Gemini |
| Firmware | VS Code + ESP-IDF + esp-bsp | — | Ya operativo (proyecto base actual) |

**Máquina de estados del firmware:**

```
┌──────┐ wake word / botón ┌──────────┐ silencio detectado ┌──────────┐
│ IDLE │ ────────────────→ │ GRABANDO │ ─────────────────→ │ ENVIANDO │
└──────┘                   └──────────┘                     └────┬─────┘
   ↑                        ↑ beep al activar                    │ respuesta
   │            ┌───────────┘                              ┌──────┴─────┐
   └────────────│ HABLANDO │ ←── audio listo ←──────────── │ PENSANDO   │
                └──────────┘                               └────────────┘
```

## 3. Modelo de asistentes

Cada asistente es **una fila de configuración, no código distinto**. El Worker enruta por `assistant_id`.

```json
{
  "id": "aura",
  "nombre": "Aura",
  "wake_word": "Aura",
  "voz": "Frida",
  "avatar": "aura.png",
  "system_prompt": "Eres Aura, cálida y serena. Detecta el nivel de estrés aparente en la voz del usuario y adapta tu tono y ritmo...",
  "historial_turnos": 20
}
```

**Trío sugerido** (nombres a tu gusto):

| Asistente | Personalidad | Tema | Voz Groq (candidatas) |
|---|---|---|---|
| Aura | Cálida, serena, empática | Bienestar, anti-estrés | Frida / Soledad |
| Sócrates | Reflexivo, pregunta más de lo que afirma | Filosofía | Armando / Valentino |
| Ada | Energética, curiosa, geek | Tendencias en IA | (tercera voz distinta) |

**Almacenamiento:**
- **KV** (config): `assistant:{id}` → JSON. `assistants` → lista de ids.
- **D1** (historial):

```sql
CREATE TABLE messages (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  assistant_id TEXT NOT NULL,
  role TEXT NOT NULL,        -- 'user' | 'assistant'
  content TEXT NOT NULL,
  created_at INTEGER NOT NULL
);
```

## 4. Contrato de la API del Worker

| Endpoint | Método | Función |
|---|---|---|
| `/chat` | POST (multipart) | Recibe `audio` (WAV 16kHz/16-bit/mono) + `assistant_id`. Devuelve `{ "transcript", "reply", "emotion", "audio_b64" }` |
| `/assistants` | GET | Lista de configs (para que el Tab5 renderice avatares/nombres) |
| `/admin` | GET | Panel web de configuración |
| `/admin/assistants/:id` | PUT | Actualiza config de un asistente |
| `/echo` | POST | Devuelve el binario intacto (para pruebas de Fase 0) |

**Seguridad:**
- Llaves de Gemini y Groq **solo** en secrets del Worker (`wrangler secret put`). Nunca en el firmware.
- El Tab5 envía un `DEVICE_TOKEN` (string aleatorio largo) compartido con el Worker como secret.
- Auth del panel `/admin`: token de administración distinto.

## 5. Preliminares — validar el cerebro desde la Mac (1–2 sesiones)

Todo desde terminal, sin escribir firmware. Elimina incertidumbre antes de pelear con hardware.

- [ ] **P.1 Cuentas:** Cloudflare, Google AI Studio (llave Gemini), Groq. Crear `DEVICE_TOKEN` aleatorio y guardarlo.
- [ ] **P.2 Voces:** curl a Groq TTS con 3–4 voces en español; elegir las del trío. *Listo cuando: escuchaste los WAVs y elegiste.*
- [ ] **P.3 Gemini audio-in:** grabar 5s de voz en la Mac, curl a Gemini con un system prompt pidiendo JSON `{reply, user_emotion}`. *Listo cuando: devuelve transcripción + respuesta + emoción correctas.*
- [ ] **P.4 Worker eco:** desplegar Worker con `POST /echo` que regrese el binario. Probar con curl. *Listo cuando: el WAV recibido suena igual al enviado.*

```bash
# P.2 — prueba de voces
curl https://api.groq.com/openai/v1/audio/speech \
  -H "Authorization: Bearer $GROQ_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{"model":"playai-tts","voice":"Frida","input":"Hola, soy tu asistente. ¿Cómo te sientes hoy?","response_format":"wav"}' \
  -o frida.wav
```

## 6. Fase 0 — Cimientos de audio en el Tab5 (2–4 sesiones)

*Requisito: tu firmware actual es el proyecto base. Si no tiene git: commit cero hoy.*

- [ ] **M0.1 Loopback:** agregar `esp-bsp` (soporte de placa `m5stack_tab5`) y hacer micrófono → altavoz con buffer en PSRAM.
  ✔ Listo cuando: susurras y se escucha tu susurro.
  🎓 Aprendes: esp_codec_dev, I2S, buffers.
- [ ] **M0.2 Grabar WAV:** capturar 5s a 16kHz/16-bit/mono en PSRAM, escribir encabezado WAV de 44 bytes, reproducir.
  ✔ Listo cuando: oyes tu propia grabación.
- [ ] **M0.3 Subida HTTPS:** POST multipart del WAV a `/echo` del Worker.
  ✔ Listo cuando: el archivo que regresa suena igual al enviado.
  🎓 Aprendes: esp_http_client, TLS, multipart.

## 7. Fase 1 — MVP: tubo de voz con un asistente (3–5 sesiones)

- [ ] **M1.1 Worker `/chat`:** recibe audio + `assistant_id` + token → carga config de KV → Gemini (audio + system prompt) → Groq TTS con la voz configurada → responde JSON con `audio_b64`. Probar primero con curl y un WAV, sin Tab5.
  ✔ Listo cuando: el curl completo funciona de punta a punta.
- [ ] **M1.2 Push-to-talk en Tab5:** botón grande en pantalla; mantener para hablar, soltar para enviar; parsear respuesta; decodificar base64 → WAV → I2S; mostrar texto en pantalla.
  ✔ Listo cuando: mantienes, hablas, sueltas → el asistente responde con voz. **(Este es el momento MVP del proyecto.)**
- [ ] **M1.3 Robustez:** estados visuales (pensando/hablando), manejo de errores de red, reintentos.
  ✔ Listo cuando: si pierdes WiFi a mitad, muestra error amigable y se recupera.
- [ ] **M1.4 Personalidad v1:** config de "Aura" en KV + avatar estático en pantalla.
  ✔ Listo cuando: la respuesta refleja el system prompt y se ve su avatar.

## 8. Fase 2 — Wake word y multi-asistente (4–8 sesiones)

- [ ] **M2.1 ⚠️ EXPERIMENTO AISLADO — Porcupine en ESP32-P4:** compilar el demo del SDK con keywords "Sistema" + nombres de los asistentes. **Puerta de decisión:** si funciona → M2.2. Si no hay build para P4 → fallback permanente: push-to-talk mejorado + selector visual, y se sigue con M2.3 igual. *No bloquea nada del proyecto.*
  ✔ Listo cuando: sabes con certeza sí o no.
- [ ] **M2.2 Escucha continua:** detección siempre activa + beep al activar + pantalla que "despierta".
  ✔ Listo cuando: dices "Sistema" al aire y responde.
- [ ] **M2.3 Multi-asistente:** 3 configs en KV, 3 wake words, routing, 3 voces claramente distintas, avatar por asistente.
  ✔ Listo cuando: dices "Sócrates..." y responde Sócrates con SU voz.
- [ ] **M2.4 VAD (fin de voz automático):** umbral de energía/silencio para cortar la grabación sin mantener botón.
  ✔ Listo cuando: hablas naturalmente y corta cuando dejas de hablar.

## 9. Fase 3 — Cerebro y personalización (continua)

- [ ] **M3.1 Memoria:** historial en D1; el Worker inyecta los últimos N turnos (según config) en cada llamada. Comando de voz "olvida esta conversación" para limpiar.
  ✔ Listo cuando: pregunta "¿qué te dije ayer?" y lo recuerda.
- [ ] **M3.2 Panel web `/admin`:** HTML servido por el Worker, protegido por token, para editar nombre, voz, wake word, prompt, avatar y turnos de memoria desde PC o celular.
  ✔ Listo cuando: cambias el prompt desde el celular y el asistente cambia de comportamiento.
- [ ] **M3.3 Habilidades:**
  - Aura consciente del estrés: Gemini ya devuelve `emotion`; usarla para reportar el nivel y ajustar tono.
  - Respiración guiada 4-7-8 (pacing de texto/tonos en el dispositivo).
  - Timers durante la conversación (nota: **no** alarmas persistentes — requerirían un canal push que no existe; documentarlo como limitación).
- [ ] **M3.4 (opcional) Ajustes en pantalla:** menú de configuración desde el propio Tab5 vía `/assistants` + PUT.
- [ ] **M3.5 (opcional) Energía:** atenuar pantalla tras inactividad (batería limitada).

## 10. Fase 4 — Cámara (opcional · requiere compra ~US$20–25)

- [ ] **M4.1 Visión:** módulo cámara oficial de Tab5 → capturar JPEG → extender `/chat` con imagen opcional → Gemini Vision. *Verificar soporte en esp-bsp antes de comprar.*
  ✔ Listo cuando: "¿qué ves?" devuelve una descripción correcta de la escena.
- [ ] **M4.2 Casos de uso combinados:** describir escena, leer notas escritas, "¿cómo me veo hoy?" cruzando imagen + tono de voz.

## 11. Presupuesto

| Concepto | Costo |
|---|---|
| Cloudflare Workers + KV + D1 (free tier) | $0 |
| Gemini 2.5 Flash (free tier, ~250 req/día) | $0 |
| Groq (TTS + fallback LLM) | $0 |
| Porcupine uso personal | $0 |
| **Estado estable del proyecto** | **$0/mes** |
| *Opcional:* ElevenLabs TTS premium (voces "wow") | ~$5/mes |
| *Opcional:* módulo cámara (pago único) | ~US$20–25 |
| *Colchón:* un mes de Gemini pago si cambian los tiers | dentro del margen de $10 |

## 12. Riesgos y mitigaciones

| # | Riesgo | Prob. | Mitigación |
|---|---|---|---|
| 1 | Porcupine sin build para ESP32-P4 | Media | Experimento aislado M2.1; fallback push-to-talk ya funcional |
| 2 | Golpear límites de free tier | Media | Flash-Lite (1000 req/día) o Groq, sin cambiar arquitectura |
| 3 | Latencia 2–5 s por turno | Cierta | Aceptable para conversación; optimizar después (streaming) |
| 4 | Voces de Groq no convencen | Baja | MeloTTS en el propio Worker (verificar voces ES) o ElevenLabs $5 |
| 5 | Quirks de I2S/mic en el Tab5 | Baja | Atacado en Fase 0, lo más temprano posible |
| 6 | Proveedores cambian condiciones | Media | El Worker es el único punto de integración: cambiar proveedor detrás del mismo contrato `/chat` |
| 7 | Free tiers de IA cambian con el tiempo | Cierta | Revisar cuatrimestralmente; el diseño multi-proveedor lo absorbe |

## 13. Reglas de trabajo (vibecoding + aprendizaje)

1. **Un hito por sesión.** Si el LLM quiere "mejorar" otras cosas: rechazar.
2. **Git desde el commit cero.** Commit al terminar cada hito, aunque sea desde terminal. Sin excepciones.
3. **Cuando algo falle, pedir explicación antes de pedir la corrección.** Ahí está el aprendizaje real (I2S, TLS, Workers, límites de runtime).
4. Mantener los prompts/consultas de las sesiones de vibecoding en `/docs/sesiones/` — sirve de bitácora.
5. Cada hito se cierra **solo** con su criterio "✔ Listo cuando", no con "ya compila".

## 14. Glosario rápido

**STT** (speech-to-text) · **LLM** (modelo de lenguaje) · **TTS** (texto a voz) · **VAD** (detección de actividad de voz) · **Wake word** (palabra de activación) · **I2S** (bus de audio digital) · **PSRAM** (RAM externa del P4, 32 MB) · **KV** (almacenamiento clave-valor de Cloudflare) · **D1** (SQLite en Cloudflare) · **Worker** (función serverless en el edge) · **multipart** (formato de POST con archivos) · **base64** (binario codificado en texto) · **system prompt** (personalidad del asistente) · **free tier** (capa gratuita)

---

**Primer paso, hoy:** P.1 (crear las tres cuentas) + P.2 (elegir voces). Todo lo demás se construye sobre eso.