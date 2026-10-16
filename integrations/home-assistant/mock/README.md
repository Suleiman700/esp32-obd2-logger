# Mock ESP32 data (test n8n / Home Assistant without the car)

| File | Use |
|---|---|
| `n8n_mock_esp_stream.json` | n8n workflow: every 10 s it POSTs a fake ESP32 batch (5 records, drive cycle, engine warming up) to your webhook. Import → set the URL in **POST like the ESP32** to your `…/webhook/car-obd` → Activate (or click **Run once**). Deactivate when done. |
| `n8n_pin_webhook_log.json` | Pin data for the **Webhook** node: open the node → Output → **Edit / Pin data** → paste. Lets you click "Test step" on the following nodes without sending anything. |
| `sample_log.json` | One ESP32 data batch (exact format). |
| `sample_alert.json` | One ESP32 alert (coolant above 105). |

Send the samples with curl (replace the URL):
```bash
curl -X POST -H "Content-Type: application/json" -d @sample_log.json   https://YOUR-N8N/webhook/car-obd
curl -X POST -H "Content-Type: application/json" -d @sample_alert.json https://YOUR-N8N/webhook/car-alert
```
Use the **/webhook/** (production, workflow active) or **/webhook-test/** (editor "Listen for test event") URL.
