# Wetterballon-Projekt 2025 am Landesgymnasium Sankt Afra

## Firmware-Architektur

Die Architektur basiert auf der Hexagonal Architecture. Dabei wird die zentrale Logik von externen Systemen, hier der Hardware von Sensoren und Funk, getrennt.

- **Ports** sind die generischen Schnittstellen, die die von der Anwendung benötigten Funktionen beschreiben.
- **Adapter** sind die hardwarespezifischen / emulierten Implementierungen.

Unter `components` sind die Ports mit ihren Adaptern definiert. Die Struktur sieht dabei folgendermaßen aus:

```text
components/
|- port.h
|- adapter1
|-- CMakeList.txt
|-- adapter.c
|-- adapter.h
|-- ...
|- adapter2
|-- ...
```
