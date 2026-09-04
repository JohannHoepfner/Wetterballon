# Wetterballon-Projekt 2025 am Landesgymnasium Sankt Afra

## Firmware-Architektur

Die Architektur basiert auf der Hexagonal Architecture. Dabei wird die zentrale Logik von externen Systemen, hier der Hardware von Sensoren und Funk, getrennt.

Die Firmware enthält folgende Schichten:

- **Domain** (`components/domain`) enthält portierbare Messkonzepte und Berechnungen, wie beispielsweise die PT100-Umwandlung.
- **Ports** (`components/ports`) enthält die generischen Schnittstellen, die die von der Anwendung benötigten Funktionen beschreiben.
- **App** (`components/app`) enthält die Arbeitsabläufe.
- **Adapter** (`components/adapter`) enthält die hardwarespezifischen / emulierten Implementierungen.

```text
domain <- ports <- app
					^
					|
				adapters
```
