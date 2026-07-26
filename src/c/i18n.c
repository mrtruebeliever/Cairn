#include "i18n.h"
#include <string.h>

// NL is the canonical source for the coaching copy; the other languages follow
// it. Keep every row the same length as the enum in i18n.h.
static const char *TR[NUM_LANGS][NUM_STRS] = {
  [L_EN] = {
    "Cairn",

    "Streak: %d d", "No streak yet", "Last: %d", "No sessions yet",
    "Finish a session to lay your first stone.",
    "SELECT · start", "UP/DOWN · pattern", "hold UP · history",
    "hold DOWN · settings", "hold SELECT · progress",

    "Coherent", "Box", "4-7-8", "Custom",

    "Breathe in", "Hold", "Breathe out",
    "%d bpm",
    "Finding your pulse", "SELECT · start now",
    "Wear the watch a little tighter, just above the wrist bone.",
    "Stop session?", "SELECT · stop  BACK · continue",
    "Paused", "SELECT · resume",

    "Session done", "Calm", "%d  %d bpm", "no heart rate",
    "New best score!", "%d days in a row", "First day",
    "New badge!", "SELECT · close",

    "Settings", "Pattern", "Custom rhythm", "Duration", "Sound",
    "Vibration", "Backlight", "Heart rate", "Reset stats",
    "On", "Off", "Timeout", "Always",
    "Soft", "Normal", "Loud",
    "Press again to erase", "SELECT · change  BACK · close",
    "SELECT · next value",
    "%d min",

    "History", "No sessions yet",

    "Progress", "Calm", "Rest HR", "Streak",
    "7 days", "30 days", "Tip of the week", "not enough data",
    "watch data only",

    "First breath", "Steady", "Zen week", "Deep calm", "Century", "Tide",
    "1 session", "3-day streak", "7-day streak", "Score 90 or more",
    "100 sessions", "1000 minutes",

    "New best score! You're finding your rhythm.",
    "No heart rate this time - wear the watch a little tighter, just above the wrist bone.",
    "Your heart rate climbed. Close your eyes and find a quieter spot before you start.",
    "You started rushed. Box breathing (4-4-4-4) usually calms fastest.",
    "Strong relaxation - your heart rate dropped a lot. Keep this rhythm.",
    "Heart rate stayed flat. Make your exhale longer than your inhale (try 4 in / 6 out).",
    "Nice coherence: your heart rate waves along with your breath. This is your sweet spot.",
    "Slow down a little - around 5-6 breaths per minute your heart rate waves the most.",
    "Short but good. One minute longer next time for more effect.",
    "Full session finished - strong. Consistency beats length.",

    "Your calm score is rising. Your nervous system is learning this - keep going.",
    "Your scores are slipping. Pick a fixed moment (before bed, say) for more regularity.",
    "You're on a plateau. Switch patterns - try 4-7-8 for deeper relaxation.",
    "Your resting heart rate is slowly dropping - regular breathwork is paying off.",
    "Resting heart rate is climbing. Stress, sleep or caffeine? Extra sessions can help.",
    "%d days in a row! Building the habit is the biggest win.",
    "Streak broken. One 2-minute session today gets it going again.",
    "Quiet week. Even one minute a day keeps the habit alive.",
    "You often stop early. Set a shorter duration so you finish; build up from there.",
    "You score highest with %s - set as your recommendation.",
  },

  [L_NL] = {
    "Cairn",

    "Reeks: %d d", "Nog geen reeks", "Laatste: %d", "Nog geen sessie",
    "Rond een sessie af en leg je eerste steen.",
    "SELECT · start", "OMHOOG/OMLAAG · patroon", "lang OMHOOG · historie",
    "lang OMLAAG · instellingen", "lang SELECT · voortgang",

    "Coherent", "Box", "4-7-8", "Eigen",

    "Adem in", "Vasthouden", "Adem uit",
    "%d bpm",
    "Hartslag zoeken", "SELECT · nu starten",
    "Draag je horloge iets strakker, net boven het polsbeen.",
    "Sessie stoppen?", "SELECT · stoppen  TERUG · verder",
    "Gepauzeerd", "SELECT · verder",

    "Sessie klaar", "Kalmte", "%d  %d bpm", "geen hartslag",
    "Nieuwe topscore!", "%d dagen op rij", "Eerste dag",
    "Nieuwe badge!", "SELECT · sluiten",

    "Instellingen", "Patroon", "Eigen ritme", "Duur", "Geluid",
    "Trilling", "Verlichting", "Hartslag", "Stats wissen",
    "Aan", "Uit", "Time-out", "Altijd",
    "Zacht", "Normaal", "Luid",
    "Nogmaals om te wissen", "SELECT · wijzigen  TERUG · sluiten",
    "SELECT · volgende waarde",
    "%d min",

    "Historie", "Nog geen sessies",

    "Voortgang", "Kalmte", "Rust-HR", "Reeks",
    "7 dagen", "30 dagen", "Tip van de week", "te weinig gegevens",
    "alleen horlogedata",

    "Eerste adem", "Standvastig", "Zen-week", "Diepe rust", "Eeuw", "Getij",
    "1 sessie", "3-daagse reeks", "7-daagse reeks", "Score 90 of meer",
    "100 sessies", "1000 minuten",

    "Nieuwe topscore! Je vindt je ritme.",
    "Geen hartslagmeting deze keer - draag je horloge iets strakker, net boven het polsbeen.",
    "Je hartslag liep op. Sluit je ogen en zoek een rustigere plek voor je start.",
    "Je begon gejaagd. Box-ademhaling (4-4-4-4) kalmeert vaak het snelst.",
    "Sterke ontspanning - je hartslag daalde flink. Hou dit ritme vast.",
    "Hartslag bleef stabiel. Maak je uitademing langer dan je inademing (bv. 4 in / 6 uit).",
    "Mooie coherentie: je hartslag golft mee met je adem. Dit is je sweet spot.",
    "Vertraag iets - rond 5-6 ademhalingen per minuut golft je hartslag het sterkst mee.",
    "Kort maar goed. Volgende keer één minuut langer voor meer effect.",
    "Volledige sessie afgerond - sterk. Consistentie telt meer dan lengte.",

    "Je kalmte-score stijgt. Je zenuwstelsel leert dit - ga zo door.",
    "Je scores zakken wat. Kies een vast moment (bv. voor het slapen) voor meer regelmaat.",
    "Je zit op een plateau. Wissel van patroon - probeer 4-7-8 voor diepere ontspanning.",
    "Je rust-hartslag daalt langzaam - regelmatig ademwerk slaat aan.",
    "Rust-hartslag loopt op. Stress, slaap of cafeïne? Extra sessies kunnen helpen.",
    "%d dagen op rij! Gewoontevorming is de grootste winst.",
    "Reeks onderbroken. Eén sessie van 2 minuten vandaag zet 'm weer op gang.",
    "Rustige week. Zelfs 1 minuut per dag houdt de gewoonte levend.",
    "Je stopt vaak vroeg. Zet de sessieduur korter zodat je 'm afmaakt; bouw daarna op.",
    "Je scoort het hoogst met %s - ingesteld als je aanrader.",
  },

  [L_FR] = {
    "Cairn",

    "Série : %d j", "Pas encore de série", "Dernier : %d", "Aucune séance",
    "Termine une séance pour poser ta première pierre.",
    "SELECT · démarrer", "HAUT/BAS · schéma", "HAUT long · historique",
    "BAS long · réglages", "SELECT long · progrès",

    "Cohérent", "Box", "4-7-8", "Perso",

    "Inspirez", "Retenez", "Expirez",
    "%d bpm",
    "Recherche du pouls", "SELECT · démarrer",
    "Serre un peu la montre, juste au-dessus de l'os du poignet.",
    "Arrêter la séance ?", "SELECT · arrêter  RETOUR · continuer",
    "Pause", "SELECT · reprendre",

    "Séance terminée", "Calme", "%d  %d bpm", "pas de fréquence",
    "Nouveau record !", "%d jours de suite", "Premier jour",
    "Nouveau badge !", "SELECT · fermer",

    "Réglages", "Schéma", "Rythme perso", "Durée", "Son",
    "Vibration", "Éclairage", "Pouls", "Effacer les stats",
    "Activé", "Désactivé", "Minuteur", "Toujours",
    "Doux", "Normal", "Fort",
    "Encore pour effacer", "SELECT · modifier  RETOUR · fermer",
    "SELECT · valeur suivante",
    "%d min",

    "Historique", "Aucune séance",

    "Progrès", "Calme", "Pouls repos", "Série",
    "7 jours", "30 jours", "Astuce de la semaine", "données insuffisantes",
    "données de la montre",

    "Premier souffle", "Constant", "Semaine zen", "Calme profond", "Centurion", "Marée",
    "1 séance", "Série de 3 jours", "Série de 7 jours", "Score 90 ou plus",
    "100 séances", "1000 minutes",

    "Nouveau record ! Tu trouves ton rythme.",
    "Pas de mesure du pouls cette fois - serre un peu la montre, juste au-dessus de l'os du poignet.",
    "Ton pouls est monté. Ferme les yeux et cherche un endroit plus calme avant de commencer.",
    "Tu as commencé agité. La respiration box (4-4-4-4) calme souvent le plus vite.",
    "Belle détente - ton pouls a bien baissé. Garde ce rythme.",
    "Pouls stable. Allonge l'expiration par rapport à l'inspiration (ex. 4 in / 6 out).",
    "Belle cohérence : ton pouls ondule avec ton souffle. C'est ton point idéal.",
    "Ralentis un peu - vers 5-6 respirations par minute, le pouls ondule le plus.",
    "Court mais bien. Une minute de plus la prochaine fois.",
    "Séance complète terminée - bravo. La régularité compte plus que la durée.",

    "Ton score de calme monte. Ton système nerveux apprend - continue.",
    "Tes scores baissent un peu. Choisis un moment fixe (avant le coucher) pour plus de régularité.",
    "Tu es sur un plateau. Change de schéma - essaie le 4-7-8 pour une détente plus profonde.",
    "Ton pouls au repos baisse lentement - la respiration régulière porte ses fruits.",
    "Le pouls au repos monte. Stress, sommeil ou caféine ? Des séances en plus peuvent aider.",
    "%d jours de suite ! Créer l'habitude est le plus grand gain.",
    "Série interrompue. Une séance de 2 minutes aujourd'hui la relance.",
    "Semaine calme. Même une minute par jour garde l'habitude vivante.",
    "Tu arrêtes souvent tôt. Réduis la durée pour finir ; augmente ensuite.",
    "Ton meilleur score est avec %s - défini comme recommandation.",
  },

  [L_DE] = {
    "Cairn",

    "Serie: %d T", "Noch keine Serie", "Letzte: %d", "Noch keine Sitzung",
    "Beende eine Sitzung und lege deinen ersten Stein.",
    "SELECT · Start", "HOCH/RUNTER · Muster", "HOCH lang · Verlauf",
    "RUNTER lang · Einstellungen", "SELECT lang · Fortschritt",

    "Kohärent", "Box", "4-7-8", "Eigen",

    "Einatmen", "Halten", "Ausatmen",
    "%d bpm",
    "Puls wird gesucht", "SELECT · jetzt starten",
    "Trage die Uhr etwas enger, knapp über dem Handgelenkknochen.",
    "Sitzung beenden?", "SELECT · beenden  ZURÜCK · weiter",
    "Pausiert", "SELECT · weiter",

    "Sitzung fertig", "Ruhe", "%d  %d bpm", "kein Puls",
    "Neuer Bestwert!", "%d Tage in Folge", "Erster Tag",
    "Neues Abzeichen!", "SELECT · schließen",

    "Einstellungen", "Muster", "Eigener Takt", "Dauer", "Ton",
    "Vibration", "Beleuchtung", "Puls zeigen", "Statistik löschen",
    "An", "Aus", "Zeitlimit", "Immer",
    "Leise", "Normal", "Laut",
    "Nochmal zum Löschen", "SELECT · ändern  ZURÜCK · schließen",
    "SELECT · nächster Wert",
    "%d Min",

    "Verlauf", "Noch keine Sitzungen",

    "Fortschritt", "Ruhe", "Ruhepuls", "Serie",
    "7 Tage", "30 Tage", "Tipp der Woche", "zu wenig Daten",
    "nur Uhr-Daten",

    "Erster Atem", "Beständig", "Zen-Woche", "Tiefe Ruhe", "Jahrhundert", "Gezeiten",
    "1 Sitzung", "3-Tage-Serie", "7-Tage-Serie", "Wert 90 oder mehr",
    "100 Sitzungen", "1000 Minuten",

    "Neuer Bestwert! Du findest deinen Rhythmus.",
    "Diesmal keine Pulsmessung - trage die Uhr etwas enger, knapp über dem Handgelenkknochen.",
    "Dein Puls stieg an. Schließe die Augen und suche vorher einen ruhigeren Ort.",
    "Du hast gehetzt angefangen. Box-Atmung (4-4-4-4) beruhigt oft am schnellsten.",
    "Starke Entspannung - dein Puls ist deutlich gesunken. Halte diesen Rhythmus.",
    "Puls blieb stabil. Atme länger aus als ein (z. B. 4 ein / 6 aus).",
    "Schöne Kohärenz: dein Puls schwingt mit dem Atem. Das ist dein Optimum.",
    "Etwas langsamer - bei 5-6 Atemzügen pro Minute schwingt der Puls am stärksten.",
    "Kurz, aber gut. Nächstes Mal eine Minute länger.",
    "Volle Sitzung geschafft - stark. Regelmäßigkeit zählt mehr als Länge.",

    "Dein Ruhe-Wert steigt. Dein Nervensystem lernt das - weiter so.",
    "Deine Werte sinken etwas. Wähle eine feste Zeit (z. B. vor dem Schlafen).",
    "Du bist auf einem Plateau. Wechsle das Muster - probiere 4-7-8 für tiefere Ruhe.",
    "Dein Ruhepuls sinkt langsam - regelmäßige Atemarbeit wirkt.",
    "Ruhepuls steigt. Stress, Schlaf oder Koffein? Zusätzliche Sitzungen helfen.",
    "%d Tage in Folge! Die Gewohnheit ist der größte Gewinn.",
    "Serie unterbrochen. Eine 2-Minuten-Sitzung heute bringt sie zurück.",
    "Ruhige Woche. Schon eine Minute pro Tag hält die Gewohnheit lebendig.",
    "Du hörst oft früh auf. Stelle die Dauer kürzer, damit du fertig wirst.",
    "Am besten schneidest du mit %s ab - als Empfehlung gesetzt.",
  },

  [L_ES] = {
    "Cairn",

    "Racha: %d d", "Aún sin racha", "Última: %d", "Aún sin sesiones",
    "Termina una sesión y coloca tu primera piedra.",
    "SELECT · empezar", "ARRIBA/ABAJO · patrón", "ARRIBA largo · historial",
    "ABAJO largo · ajustes", "SELECT largo · progreso",

    "Coherente", "Box", "4-7-8", "Propio",

    "Inspira", "Retén", "Espira",
    "%d bpm",
    "Buscando el pulso", "SELECT · empezar ya",
    "Ajusta el reloj algo más, justo encima del hueso de la muñeca.",
    "¿Parar la sesión?", "SELECT · parar  ATRÁS · seguir",
    "Pausa", "SELECT · seguir",

    "Sesión lista", "Calma", "%d  %d bpm", "sin pulso",
    "¡Nuevo récord!", "%d días seguidos", "Primer día",
    "¡Nueva insignia!", "SELECT · cerrar",

    "Ajustes", "Patrón", "Ritmo propio", "Duración", "Sonido",
    "Vibración", "Luz", "Mostrar pulso", "Borrar estadísticas",
    "Activado", "Desactivado", "Con tiempo", "Siempre",
    "Bajo", "Normal", "Alto",
    "Otra vez para borrar", "SELECT · cambiar  ATRÁS · cerrar",
    "SELECT · valor siguiente",
    "%d min",

    "Historial", "Aún sin sesiones",

    "Progreso", "Calma", "Pulso rep.", "Racha",
    "7 días", "30 días", "Consejo de la semana", "datos insuficientes",
    "solo datos del reloj",

    "Primer aliento", "Constante", "Semana zen", "Calma profunda", "Centenario", "Marea",
    "1 sesión", "Racha de 3 días", "Racha de 7 días", "Puntuación 90 o más",
    "100 sesiones", "1000 minutos",

    "¡Nuevo récord! Estás encontrando tu ritmo.",
    "Sin medición de pulso esta vez - ajusta el reloj algo más, justo encima del hueso.",
    "Tu pulso subió. Cierra los ojos y busca un sitio más tranquilo antes de empezar.",
    "Empezaste acelerado. La respiración box (4-4-4-4) suele calmar más rápido.",
    "Buena relajación - tu pulso bajó bastante. Mantén este ritmo.",
    "El pulso se mantuvo estable. Alarga la espiración más que la inspiración (4 in / 6 out).",
    "Buena coherencia: tu pulso ondula con tu respiración. Este es tu punto óptimo.",
    "Ve algo más lento - sobre 5-6 respiraciones por minuto el pulso ondula más.",
    "Corto pero bueno. La próxima vez un minuto más.",
    "Sesión completa terminada - bien. La constancia cuenta más que la duración.",

    "Tu puntuación de calma sube. Tu sistema nervioso lo está aprendiendo - sigue así.",
    "Tus puntuaciones bajan algo. Elige un momento fijo (antes de dormir) para más constancia.",
    "Estás en una meseta. Cambia de patrón - prueba 4-7-8 para una relajación más profunda.",
    "Tu pulso en reposo baja poco a poco - la respiración regular funciona.",
    "El pulso en reposo sube. ¿Estrés, sueño o cafeína? Más sesiones pueden ayudar.",
    "¡%d días seguidos! Crear el hábito es la mayor ganancia.",
    "Racha interrumpida. Una sesión de 2 minutos hoy la reactiva.",
    "Semana tranquila. Incluso un minuto al día mantiene el hábito.",
    "Sueles parar pronto. Pon una duración más corta para terminarla; luego sube.",
    "Tu mejor puntuación es con %s - fijado como recomendación.",
  },
};

const char **s_tr = TR[L_EN];

void i18n_init(void) {
  const char *loc = i18n_get_system_locale();
  if (!loc) return;
  if (strncmp(loc, "nl", 2) == 0)      s_tr = TR[L_NL];
  else if (strncmp(loc, "fr", 2) == 0) s_tr = TR[L_FR];
  else if (strncmp(loc, "de", 2) == 0) s_tr = TR[L_DE];
  else if (strncmp(loc, "es", 2) == 0) s_tr = TR[L_ES];
  else                                 s_tr = TR[L_EN];
}
