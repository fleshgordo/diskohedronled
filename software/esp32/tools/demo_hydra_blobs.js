// DEMO Hydra + p5 – Noise-Blobs aus p5 werden durch Hydra gejagt
// Hydra (Live-Coding Visuals): https://hydra.ojack.xyz  ·  Doku: https://hydra.ojack.xyz/docs
//
// index.html:
//   <script src="https://cdnjs.cloudflare.com/ajax/libs/p5.js/1.11.10/p5.min.js"></script>
//   <script src="https://unpkg.com/hydra-synth@1.4.0/dist/hydra-synth.js"></script>
//
// Prinzip:  p5 zeichnet (versteckt) die Blobs  ->  Hydra nimmt den p5-Canvas als Quelle s0
//           und verarbeitet ihn mit Shader-Effekten weiter  ->  sichtbar ist nur Hydra.
//
// Tasten 1–5: Hydra-Effekt wechseln   ·   Maus X/Y: steuert die Effekte
//
// ACHTUNG Pain-Point: Hydra und p5 haben gleichnamige Funktionen (noise, shape, src ...).
// Darum makeGlobal: false -> alle Hydra-Funktionen laufen über "h." (h.osc(), h.noise() ...)

let h; // Hydra-Synth
let blobs = [];

function setup() {
  createCanvas(windowWidth, windowHeight);
  pixelDensity(1);
  select("canvas").hide(); // p5-Canvas ist nur die Quelle, nicht sichtbar

  // eigener Canvas für Hydra
  let hc = createElement("canvas");
  hc.elt.width = width;
  hc.elt.height = height;
  hc.style("display", "block");
  let hydra = new Hydra({
    canvas: hc.elt,
    width: width,
    height: height,
    detectAudio: false,
    makeGlobal: false,
  });
  h = hydra.synth;

  // p5-Canvas als Hydra-Quelle s0
  h.s0.init({ src: drawingContext.canvas });

  // ein paar Blobs mit eigener Farbe und eigenem Noise-Offset
  let palette = ["#ff3d9a", "#00e0a0", "#3d7bff", "#ffd400"];
  for (let i = 0; i < 6; i++) {
    blobs.push({
      seed: random(1000),
      r: random(80, 180),
      col: color(palette[i % palette.length]),
    });
  }

  setEffect(1);
}

function draw() {
  background(0);
  noStroke();
  let t = frameCount * 0.004;

  for (let b of blobs) {
    // Position wandert per noise über den Screen
    let x = noise(b.seed, t) * width;
    let y = noise(b.seed + 50, t) * height;

    // Umriss: Radius pro Winkel per noise -> organische Form
    fill(b.col);
    beginShape();
    for (let a = 0; a < TWO_PI; a += 0.1) {
      let n = noise(b.seed + cos(a), b.seed + sin(a), t * 3);
      let r = b.r * map(n, 0, 1, 0.5, 1.4);
      curveVertex(x + cos(a) * r, y + sin(a) * r);
    }
    endShape(CLOSE);
  }
}

// ---------- Hydra-Effekte ----------
// Hydra liest Maus/Zeit selbst über Funktionen: () => wert
function setEffect(n) {
  const mx = () => mouseX / width; // 0..1
  const my = () => mouseY / height;

  if (n === 1) {
    // pur: nur leicht verwischte Spur (Feedback mit o0)
    h.src(h.s0).blend(h.src(h.o0).scale(1.01), 0.85).out();
  }
  if (n === 2) {
    // Flüssig: noise verbiegt das Bild
    h.src(h.s0)
      .modulate(h.noise(3, 0.2), () => 0.02 + mx() * 0.2)
      .blend(h.src(h.o0), 0.6)
      .out();
  }
  if (n === 3) {
    // Kaleidoskop – die Maus wählt, welcher Bildausschnitt gespiegelt wird
    h.src(h.s0)
      .repeat(3, 3) // Bild kacheln, damit das Kaleidoskop immer genug "Material" hat
      .scroll(() => mx(), () => my())
      .kaleid(6)
      .rotate(() => h.time * 0.1)
      .modulate(h.osc(10, 0.1), 0.03)
      .out();
  }
  if (n === 4) {
    // Glitch-Raster + Farbverschiebung
    h.src(h.s0)
      .pixelate(() => 20 + mx() * 200, () => 20 + my() * 200)
      .hue(() => h.time * 0.1)
      .diff(h.src(h.o0).scrollX(0.002))
      .out();
  }
  if (n === 5) {
    // Wiederholen + Feedback-Tunnel
    h.src(h.o0)
      .scale(1.02)
      .rotate(0.01)
      .blend(h.src(h.s0).repeat(() => 1 + Math.floor(my() * 4), 2), 0.3)
      .out();
  }
}

function keyPressed() {
  let n = int(key);
  if (n >= 1 && n <= 5) setEffect(n);
}
