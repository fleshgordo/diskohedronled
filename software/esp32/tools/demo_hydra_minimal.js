// HYDRA MINIMAL – p5 zeichnet, Hydra verbiegt es mit noise()
// index.html: p5 1.11.10 + https://unpkg.com/hydra-synth@1.4.0/dist/hydra-synth.js

let h;

function setup() {
  createCanvas(400, 400);
  pixelDensity(1);
  select("canvas").hide(); // p5-Canvas ist nur die Quelle

  let hc = createElement("canvas"); // Canvas für Hydra
  hc.elt.width = width;
  hc.elt.height = height;
  h = new Hydra({ canvas: hc.elt, detectAudio: false, makeGlobal: false }).synth;

  h.s0.init({ src: drawingContext.canvas }); // p5-Canvas -> Hydra-Quelle s0
  h.src(h.s0).modulate(h.noise(3), 0.1).out(); // <- der ganze Hydra-Teil
}

function draw() {
  background(0);
  fill(255, 0, 150);
  noStroke();
  circle(mouseX, mouseY, 150);
}
