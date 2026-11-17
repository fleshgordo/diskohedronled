// DEMO ml5.js – Emoji übers Gesicht, Smartphone wird zum 📱
// https://ml5js.org  ·  faceMesh (Gesicht) + objectDetection (COCO-SSD, erkennt "cell phone")
//
// index.html:
//   <script src="https://cdnjs.cloudflare.com/ajax/libs/p5.js/1.11.10/p5.min.js"></script>
//   <script src="https://unpkg.com/ml5@1.4.0/dist/ml5.min.js"></script>
//
// Mund öffnen -> 😮   ·   Mund zu -> 😎   ·   Handy in die Kamera halten -> 📱
// Taste D: Debug-Ansicht (Boxen + Labels) an/aus

let video;
let faceMesh;
let detector;
let faces = [];
let objects = [];
let debug = false;

// geglättete Positionen (damit die Emojis nicht zittern)
let face = { x: 0, y: 0, w: 0, h: 0, seen: false };
let phone = { x: 0, y: 0, w: 0, h: 0, seen: false, lastSeen: -999 };

function preload() {
  faceMesh = ml5.faceMesh({ maxFaces: 1, refineLandmarks: false, flipped: false });
  detector = ml5.objectDetection("cocossd");
}

function setup() {
  createCanvas(640, 480);
  video = createCapture(VIDEO);
  video.size(640, 480);
  video.hide();

  faceMesh.detectStart(video, (results) => (faces = results));
  detector.detectStart(video, (results) => (objects = results));

  textAlign(CENTER, CENTER);
}

function draw() {
  // Video gespiegelt zeichnen (wie ein Spiegel)
  push();
  translate(width, 0);
  scale(-1, 1);
  image(video, 0, 0, width, height);
  pop();

  drawFace();
  drawPhone();

  if (debug) drawDebug();
}

// ---------- Gesicht ----------
function drawFace() {
  if (faces.length > 0) {
    let b = faces[0].box;
    // x spiegeln, weil das Video gespiegelt ist
    let x = width - (b.xMin + b.width / 2);
    let y = b.yMin + b.height / 2;
    smoothTo(face, x, y, b.width, b.height, 0.4);
    face.seen = true;

    // Mund offen? Abstand Oberlippe (13) – Unterlippe (14) im Verhältnis zur Gesichtshöhe
    let k = faces[0].keypoints;
    let mouth = dist(k[13].x, k[13].y, k[14].x, k[14].y) / b.height;
    face.emoji = mouth > 0.06 ? "😮" : "😎";
  } else {
    face.seen = false;
  }

  if (face.seen) {
    textSize(face.h * 1.25);
    text(face.emoji, face.x, face.y + face.h * 0.05);
  }
}

// ---------- Smartphone ----------
function drawPhone() {
  let p = objects.find((o) => o.label === "cell phone" && o.confidence > 0.4);
  if (p) {
    let x = width - (p.x + p.width / 2);
    let y = p.y + p.height / 2;
    smoothTo(phone, x, y, p.width, p.height, phone.seen ? 0.4 : 1);
    phone.seen = true;
    phone.lastSeen = frameCount;
  } else if (frameCount - phone.lastSeen > 15) {
    phone.seen = false; // erst nach ~0.25s ausblenden, gegen Flackern
  }

  if (phone.seen) {
    textSize(max(phone.w, phone.h) * 1.1);
    text("📱", phone.x, phone.y);
  }
}

function smoothTo(o, x, y, w, h, amt) {
  if (o.w === 0) amt = 1; // erstes Mal: direkt springen
  o.x = lerp(o.x, x, amt);
  o.y = lerp(o.y, y, amt);
  o.w = lerp(o.w, w, amt);
  o.h = lerp(o.h, h, amt);
}

// ---------- Debug: was "sieht" die KI? ----------
function drawDebug() {
  noFill();
  strokeWeight(2);
  textSize(14);
  for (let o of objects) {
    let x = width - o.x - o.width;
    stroke(0, 255, 150);
    rect(x, o.y, o.width, o.height);
    noStroke();
    fill(0, 255, 150);
    text(o.label + " " + nf(o.confidence, 1, 2), x + o.width / 2, o.y - 10);
    noFill();
  }
  if (faces.length > 0) {
    noStroke();
    fill(255, 0, 150);
    for (let k of faces[0].keypoints) circle(width - k.x, k.y, 2);
  }
}

function keyPressed() {
  if (key === "d" || key === "D") debug = !debug;
}
