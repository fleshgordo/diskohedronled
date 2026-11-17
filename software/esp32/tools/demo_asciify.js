// DEMO p5.asciify – die Webcam als live ASCII-Art
// https://github.com/humanbydefinition/p5.asciify  ·  Doku: https://p5.textmode.art
//
// index.html (Reihenfolge wichtig: zuerst p5, dann p5.asciify):
//   <script src="https://cdnjs.cloudflare.com/ajax/libs/p5.js/1.11.10/p5.min.js"></script>
//   <script src="https://cdn.jsdelivr.net/npm/p5.asciify@0.10.3/dist/p5.asciify.umd.min.js"></script>
//
// Tasten:
//   C      Farbe: Webcam-Farben <-> Matrix-Grün
//   E      Kanten-Renderer (edge) an/aus
//   ↑ / ↓  Schrift grösser / kleiner (= gröbere / feinere Auflösung)
//   T      aktuelles Bild als .txt speichern

let video;
let asciifier;
let brightness;
let edge;

let fontSize = 12;
let sampledColor = true;
let edgeOn = false;

function setup() {
  createCanvas(windowWidth, windowHeight, WEBGL); // p5.asciify läuft nur in WEBGL
  video = createCapture(VIDEO);
  video.size(640, 480);
  video.hide();
}

// wird von p5.asciify automatisch nach setup() aufgerufen
function setupAsciify() {
  asciifier = p5asciify.asciifier();
  asciifier.fontSize(fontSize);
  asciifier.background("#000000");

  brightness = asciifier.renderers().get("brightness");
  brightness
    .characters(" .:-=+*#%@") // von hell nach dunkel
    .characterColorMode("sampled") // Zeichenfarbe aus dem Bild
    .backgroundColorMode("fixed")
    .backgroundColor(color(0));

  edge = asciifier.renderers().get("edge");
  edge.characters("-/|\\-/|\\").characterColor(color(255)).disable();
}

function draw() {
  background(0);

  // Webcam spiegeln und so skalieren, dass sie den Screen füllt
  let s = max(width / video.width, height / video.height);
  push();
  scale(-1, 1);
  image(video, (-video.width * s) / 2, (-video.height * s) / 2, video.width * s, video.height * s);
  pop();
}

function keyPressed() {
  if (key === "c" || key === "C") {
    sampledColor = !sampledColor;
    if (sampledColor) {
      brightness.characterColorMode("sampled");
    } else {
      brightness.characterColorMode("fixed").characterColor(color(0, 255, 100));
    }
  }
  if (key === "e" || key === "E") {
    edgeOn = !edgeOn;
    edge.enabled(edgeOn);
  }
  if (keyCode === UP_ARROW) {
    fontSize = min(fontSize + 2, 48);
    asciifier.fontSize(fontSize);
  }
  if (keyCode === DOWN_ARROW) {
    fontSize = max(fontSize - 2, 4);
    asciifier.fontSize(fontSize);
  }
  if (key === "t" || key === "T") {
    asciifier.saveStrings("ascii_selfie");
  }
}

function windowResized() {
  resizeCanvas(windowWidth, windowHeight);
}
