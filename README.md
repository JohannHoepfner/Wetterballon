# Wetterballon-Projekt 2025 am Landesgymnasium Sankt Afra

## Firmware-Architektur

Die Architektur basiert auf der Hexagonal Architecture. Dabei wird die zentrale Logik von externen Systemen, hier der Hardware von Sensoren und Funk, getrennt.

- **Ports** sind die generischen Schnittstellen, die die von der Anwendung benötigten Funktionen beschreiben.
- **Adapter** sind die hardwarespezifischen / emulierten Implementierungen.

Unter `components` sind die Ports mit ihren Adaptern definiert. Die Struktur sieht dabei folgendermaßen aus:

```text
components/
|- port/
|-- port.h
|-- adapter/
|--- include/
|---- adapter.h
|--- CMakeList.txt
|--- idf_component.yml
|--- adapter.c
```

## Übersicht Messungen

*Hinweis: Ein Adapter sollte nicht mehrfach verwendet werden, da sonst in den Daten anhand der Kennung nicht unterscheidbar ist, von welchem Adapter der Messwert stammt.*

| Kennung | Sensor | Messgröße | Einheit | Adapter |
|---------|--------|-----------|---------|---------|
| m | - | - | - | mock |
||||||
| s | Geigerzähler | Zerfallsrate | Hz | geiger |
||||||
| h | BME280 | Luftfeuchtigkeit | % relative Feuchtigkeit | bme280_s |
| t2 | BME280 | Temperatur | °C | bme280_s |
| p | BME280 | Druck | hPa | bme280_s |
||||||
| t1 | Pt1000 | Temperatur | °C | pt1000 |
||||||
| lat | NEO-m8 | Breitengrad | ° | neo_m8 |
| lon | NEO-m8 | Längengrad | ° | neo_m8 |
| alt | NEO-m8 | Höhe | m (?) | neo_m8 |
| utc | NEO-m8 | Zeit (UTC) | Uhr | neo_m8 |
