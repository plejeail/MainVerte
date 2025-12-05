# 🌿 MainVerte  
Offline-first Android app for managing plant collections  
Built with **Kotlin**, **Jetpack Compose**, and **SQLite**

---

## 📌 Overview

MainVerte is a lightweight, privacy-friendly plant management application designed for hobbyists, collectors, and indoor gardeners.

The app provides:
- Editable **species profiles**
- Personalized **specimen sheets**
- Automatic **care alerts** (water, fertilizer, light, humidity, temperature)
- User-defined **events** and journals
- Photo tracking
- Full **offline** local storage using a hand-crafted SQLite schema
- Export capabilities for both plants and global data

No cloud. No tracking. Everything stays on the device.

---

## 🧱 Tech Stack

- **Language:** Kotlin (Android)
- **UI:** Jetpack Compose
- **Database:** SQLite (strict schema, custom triggers, enum documentation tables)
- **Architecture:** data-oriented, minimal indirection, minimal dependencies
- **Min SDK:** 29 (Android 10)
- **Build system:** Gradle + Android Gradle Plugin

---

## 📂 Project Structure
app/
|- src/main/kotlin/ # App source code (screens, viewmodels, db)
|- src/main/res/ # Resources (UI, icons, strings)
|- src/main/assets/ # Bundled data (SQLite seed, images)
|- mainverte.db # Local database (created at first run)

documentation/
|- ... # App specs, data model, developer docs

database/
|- ... # SQL schema, migrations, seed scripts

---

## 🌱 Core Features

### 👉 Species Profiles  
Editable botanical profiles (family, exposure, environment, description, etc.)

### 👉 Specimen Management  
Per-plant custom sheet with:
- name  
- photo  
- environment  
- substrate  
- care thresholds  
- last watering / repotting / rotation  
- notes  

### 👉 Care Alerts  
Automatically tracked indicators:
- watering interval  
- fertilization  
- light DLI  
- humidity  
- temperature  
- repotting schedule  

Each metric supports both **warning** and **danger** thresholds.

### 👉 Journals & Events  
User-generated or system-generated care logs:
- watering
- fertilization
- pruning
- repotting
- lifecycle events
- photos

All events can be linked to one or multiple specimens.

---

## 💾 Database

The application uses a custom SQLite schema built for:
- strict data validation  
- reproducible migrations  
- minimal external dependencies  
- high performance on low-end devices  

The schema includes:
- documented enums  
- update triggers  
- species + specimen tables  
- journal/event relations  
- alert definitions and event generators  

---

## ▶️ Running the Project

1. Clone the repository
2. Run database/database_builder.ps1 to generate the database
3. Open the project in **Android Studio** (latest stable)
4. Sync Gradle
5. Build & run on Android 10+

A default SQLite database (`mainverte.db`) is bundled and initialized at first launch.

---

## 📦 Roadmap

- [ ] Complete specimen profile
- [ ] Journaling
- [ ] Data export per specimen
- [ ] Event notifications
- [ ] Global export (archive)
- [ ] Improved species search  
- [ ] Multi-image journals  
- [ ] Widgets (water reminders, plant of the day)  
- [ ] Local backup/restore  

---

## 📜 License

MIT License.  
Feel free to fork, modify, and use for personal projects.

---
