// DEMO p5.riso – generatives Riso-Poster in 3 Farbschichten
// https://github.com/antiboredom/p5.riso
//
// index.html (p5 1.x!):
//   <script src="https://cdnjs.cloudflare.com/ajax/libs/p5.js/1.11.10/p5.min.js"></script>
//   <script src="https://cdn.jsdelivr.net/gh/antiboredom/p5.riso/lib/p5.riso.js"></script>
//
// Jede Riso-Farbe ist eine eigene Ebene (wie eine Druckplatte).
// Überlagert werden sie mit MULTIPLY – genau wie Druckfarbe auf Papier.
//
// Klick      neues Poster
// Taste S    alle Farbauszüge nebeneinander als EIN PNG speichern
// Taste L    jede Farbebene als eigenes PNG speichern (= Druckvorlagen)

let pink, blue, yellow;

function setup() {
  createCanvas(420, 594); // A-Format (1 : √2)
  pixelDensity(1);

  pink = new Riso("fluorescentpink");
  blue = new Riso("blue");
  yellow = new Riso("yellow");

  noLoop();
}

function draw() {
  background(245, 240, 230); // Papierton
  clearRiso();
  risoNoStroke();

  let seed = random(1000);
  noiseSeed(seed);

  // GELB: ein grosser Kreis
  yellow.fill(255);
  yellow.circle(width * random(0.3, 0.7), height * 0.35, width * 0.8);

  // PINK: Noise-Wellen (Linien aus Kreisen, wie letzte Woche)
  for (let row = 0; row < 14; row++) {
    pink.fill(map(row, 0, 13, 60, 255)); // Deckkraft = "wie viel Farbe"
    for (let x = 0; x < width; x += 4) {
      let y = height * 0.25 + row * 22 + noise(x * 0.008, row * 0.3) * 120;
      pink.circle(x, y, 10);
    }
  }

  // BLAU: Raster aus Quadraten, Grösse per noise
  blue.fill(220);
  for (let x = 30; x < width; x += 30) {
    for (let y = height * 0.55; y < height - 60; y += 30) {
      let s = noise(x * 0.02, y * 0.02, seed) * 26;
      blue.rect(x - s / 2, y - s / 2, s, s);
    }
  }

  // Text auf der blauen Ebene
  blue.textSize(48);
  blue.textStyle(BOLD);
  blue.text("KIPRO", 30, height - 25);

  // Cutout: Pink wird dort ausgespart, wo Blau druckt (wie ein Stanzmesser)
  pink.cutout(blue);

  drawRiso();
}

function mousePressed() {
  redraw();
}

function keyPressed() {
  // S: alle Farbauszüge nebeneinander in EINEM PNG (klappt immer)
  if (key === "s" || key === "S") {
    saveSeparations();
  }
  // L: jede Ebene als eigenes PNG
  // Browser blockieren mehrere Downloads gleichzeitig -> mit Pause dazwischen.
  // Chrome fragt evtl. einmal "Mehrere Dateien herunterladen?" -> Zulassen.
  // (exportRiso() / layer.export() der Library sind buggy -> eigene Funktion saveLayer)
  if (key === "l" || key === "L") {
    Riso.channels.forEach((layer, i) => {
      setTimeout(() => saveLayer(layer, i), i * 800);
    });
  }
}

// eine Ebene als Schwarz-auf-Weiss-PNG, Dateiname = Nummer + Farbname
function saveLayer(layer, i) {
  let img = layer.get();
  img.loadPixels();
  for (let p = 0; p < img.pixels.length; p += 4) {
    img.pixels[p] = 0;
    img.pixels[p + 1] = 0;
    img.pixels[p + 2] = 0;
  }
  img.updatePixels();

  let out = createGraphics(width, height);
  out.pixelDensity(1);
  out.background(255);
  out.image(img, 0, 0);
  saveCanvas(out, "riso_" + (i + 1) + "_" + layer.channelName.toLowerCase(), "png");
  out.remove();
}

// Druckvorlagen: jede Farbe als Schwarz auf Weiss (so will es die Riso-Maschine)
function saveSeparations() {
  let n = Riso.channels.length;
  let sheet = createGraphics(width * n, height + 30);
  sheet.pixelDensity(1);
  sheet.background(255);
  sheet.fill(0);
  sheet.noStroke();
  sheet.textSize(16);

  Riso.channels.forEach((layer, i) => {
    let img = layer.get();
    img.loadPixels();
    for (let p = 0; p < img.pixels.length; p += 4) {
      img.pixels[p] = 0; // R
      img.pixels[p + 1] = 0; // G
      img.pixels[p + 2] = 0; // B  -> schwarz, Deckkraft bleibt = Farbmenge
    }
    img.updatePixels();
    sheet.image(img, i * width, 30);
    sheet.text(layer.channelName, i * width + 10, 20);
  });

  sheet.save("riso_separationen.png");
}
