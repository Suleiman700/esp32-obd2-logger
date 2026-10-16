# Car data in Home Assistant (via n8n)

```
ESP32 ──POST JSON──▶ n8n webhook ──▶ Home Assistant REST API ──▶ sensor.car_*  ──▶ dashboard
                     (car-obd)       /api/states/sensor.car_rpm …
ESP32 alerts ──────▶ n8n webhook ──▶ HA notification (persistent_notification)
                     (car-alert)
```

## 1. Home Assistant: create a token
Profile (bottom-left) → **Security** → **Long-lived access tokens** → **Create token** → copy it.

## 2. n8n: import the workflow
1. n8n → **Workflows → Import from file** → `n8n_car_to_home_assistant.json`
2. In both **HTTP Request** nodes ("Update HA sensor", "HA notification"):
   - replace `http://homeassistant.local:8123` with your Home Assistant address
   - replace `PASTE_YOUR_LONG_LIVED_TOKEN_HERE` with the token
3. **Activate** the workflow and copy the two **Production** URLs of the webhook nodes:
   `…/webhook/car-obd` and `…/webhook/car-alert`

n8n must be able to reach Home Assistant. Same network: use the local IP. n8n on a VPS: use the HA
cloud/tunnel URL (Nabu Casa, Cloudflare Tunnel …).

## 3. OBD Monitor: point it at n8n
Web panel → **Settings → Streaming**:
- **Data URL** = `…/webhook/car-obd`
- **Alerts URL** = `…/webhook/car-alert`
- Save. Pick what to send in **Favorites & Log** (the *Log* column).

## 4. Dashboard
The sensors appear after the first upload (`sensor.car_rpm`, `sensor.car_coolant`, `sensor.car_voltage`, …).
Dashboard → Edit → Add card → **Manual** → paste `ha_car_dashboard.yaml`.

## Notes
- Only the **newest live** record of each upload is sent to HA. DEMO data is ignored, only `sensor.car_obd_mode` shows `demo`.
- Alerts appear as Home Assistant notifications. For phone push, change the alert HTTP node URL to
  `/api/services/notify/mobile_app_<your_phone>` and the body to `{"title": ..., "message": ...}`.
- Sensors created through the REST API come back on the next upload after an HA restart and can't be edited
  in the UI. For permanent entities grouped as one "Car" device, MQTT discovery is the next step.
