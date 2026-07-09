const LUMINANCE_THRESHOLD = 190;

function quantize2(value) {
  var v = Math.floor((value + 42) / 85);
  if (v < 0) return 0;
  if (v > 3) return 3;
  return v;
}

function rgbaToPebbleColor(r, g, b) {
  var r2 = quantize2(r);
  var g2 = quantize2(g);
  var b2 = quantize2(b);
  return 0xc0 | (r2 << 4) | (g2 << 2) | b2;
}

function packMonochrome(imageData, width, height, bytesPerRow) {
  var packed = new Uint8Array(bytesPerRow * height);
  var data = imageData.data;
  for (var y = 0; y < height; y++) {
    for (var x = 0; x < width; x++) {
      var idx = (y * width + x) * 4;
      var r = data[idx], g = data[idx + 1], b = data[idx + 2];
      var luminance = r * 0.2126 + g * 0.7152 + b * 0.0722;
      var bit = luminance > LUMINANCE_THRESHOLD ? 1 : 0;
      if (bit) {
        var byteIndex = y * bytesPerRow + (x >> 3);
        var bitIndex = 7 - (x & 7);
        packed[byteIndex] |= 1 << bitIndex;
      }
    }
  }
  return packed;
}

function packColorRle2Bit(imageData, width, height) {
  var data = imageData.data;
  var pixelCount = width * height;
  var out = [];
  var pixel = 0;
  while (pixel < pixelCount) {
    var idx = pixel * 4;
    var color6 = rgbaToPebbleColor(data[idx], data[idx + 1], data[idx + 2]) & 0x3f;
    var run = 1;
    while (run < 4 && pixel + run < pixelCount) {
      var nextIdx = (pixel + run) * 4;
      var nextColor6 = rgbaToPebbleColor(data[nextIdx], data[nextIdx + 1], data[nextIdx + 2]) & 0x3f;
      if (nextColor6 !== color6) break;
      run += 1;
    }
    out.push(((run - 1) << 6) | color6);
    pixel += run;
  }
  return new Uint8Array(out);
}

function packMonochromeBitRle2(imageData, width, height) {
  var data = imageData.data;
  var pixelCount = width * height;
  if (pixelCount === 0) return new Uint8Array([0]);
  var bitValues = new Uint8Array(pixelCount);
  for (var p = 0; p < pixelCount; p++) {
    var idx = p * 4;
    var r = data[idx], g = data[idx + 1], b = data[idx + 2];
    var luminance = r * 0.2126 + g * 0.7152 + b * 0.0722;
    bitValues[p] = luminance > LUMINANCE_THRESHOLD ? 1 : 0;
  }
  var out = [];
  var curByte = 0;
  var bitsUsed = 0;
  function writeBits(value, bitCount) {
    for (var i = bitCount - 1; i >= 0; i--) {
      var bit = (value >> i) & 1;
      curByte = (curByte << 1) | bit;
      bitsUsed += 1;
      if (bitsUsed === 8) {
        out.push(curByte);
        curByte = 0;
        bitsUsed = 0;
      }
    }
  }
  var firstBit = bitValues[0];
  out.push(firstBit);
  var runColor = firstBit;
  var runLength = 0;
  function emitRun(length) {
    while (length > 0) {
      if (length <= 3) {
        writeBits(length - 1, 2);
        length = 0;
      } else if (length > 258) {
        writeBits(3, 2);
        writeBits(0, 8);
        length -= 258;
      } else {
        writeBits(3, 2);
        writeBits(length - 3, 8);
        length = 0;
      }
    }
  }
  for (var pixel = 0; pixel < pixelCount; pixel++) {
    var bit = bitValues[pixel];
    if (bit === runColor) {
      runLength += 1;
      continue;
    }
    emitRun(runLength);
    runColor = bit;
    runLength = 1;
  }
  emitRun(runLength);
  if (bitsUsed > 0) out.push(curByte << (8 - bitsUsed));
  return new Uint8Array(out);
}

module.exports = {
  packMonochrome: packMonochrome,
  packMonochromeBitRle2: packMonochromeBitRle2,
  packColorRle2Bit: packColorRle2Bit
};
