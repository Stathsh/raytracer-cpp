// ─── Ray Tracer Engine (Web Worker) ───────────────────────

// Vec3
class Vec3 {
  constructor(x = 0, y = 0, z = 0) { this.x = x; this.y = y; this.z = z; }
  add(v) { return new Vec3(this.x + v.x, this.y + v.y, this.z + v.z); }
  sub(v) { return new Vec3(this.x - v.x, this.y - v.y, this.z - v.z); }
  mul(t) { return typeof t === 'number' ? new Vec3(this.x * t, this.y * t, this.z * t) : new Vec3(this.x * t.x, this.y * t.y, this.z * t.z); }
  div(t) { return new Vec3(this.x / t, this.y / t, this.z / t); }
  dot(v) { return this.x * v.x + this.y * v.y + this.z * v.z; }
  cross(v) { return new Vec3(this.y * v.z - this.z * v.y, this.z * v.x - this.x * v.z, this.x * v.y - this.y * v.x); }
  length() { return Math.sqrt(this.x * this.x + this.y * this.y + this.z * this.z); }
  normalized() { const l = this.length(); return l > 0 ? this.div(l) : new Vec3(); }
  reflect(n) { return this.sub(n.mul(2.0 * this.dot(n))); }
}

function intersectSphere(ray, sphere) {
  if (!sphere.enabled) return null;
  const oc = ray.origin.sub(sphere.center);
  const a = ray.dir.dot(ray.dir);
  const b = 2.0 * oc.dot(ray.dir);
  const c = oc.dot(oc) - sphere.radius * sphere.radius;
  const disc = b * b - 4 * a * c;
  if (disc < 0) return null;
  const sqrtDisc = Math.sqrt(disc);
  let t = (-b - sqrtDisc) / (2.0 * a);
  if (t < 0.001) t = (-b + sqrtDisc) / (2.0 * a);
  if (t < 0.001) return null;
  const point = ray.origin.add(ray.dir.mul(t));
  const normal = point.sub(sphere.center).normalized();
  return { t, point, normal, material: sphere.material };
}

function intersectPlane(ray, plane) {
  const denom = plane.normal.dot(ray.dir);
  if (Math.abs(denom) < 1e-6) return null;
  const t = plane.point.sub(ray.origin).dot(plane.normal) / denom;
  if (t < 0.001) return null;
  const point = ray.origin.add(ray.dir.mul(t));
  let normal = plane.normal;
  if (denom > 0) normal = normal.mul(-1);

  let fx = Math.floor(point.x * plane.scale);
  let fz = Math.floor(point.z * plane.scale);
  if (point.x < 0) fx--;
  if (point.z < 0) fz--;
  const material = ((fx + fz) & 1) ? plane.mat2 : plane.mat1;
  return { t, point, normal, material };
}

function traceScene(ray, scene) {
  let closest = null;
  for (const s of scene.spheres) {
    const h = intersectSphere(ray, s);
    if (h && (!closest || h.t < closest.t)) closest = h;
  }
  for (const p of scene.planes) {
    const h = intersectPlane(ray, p);
    if (h && (!closest || h.t < closest.t)) closest = h;
  }
  return closest;
}

function inShadow(point, lightDir, lightDist, scene) {
  const ray = { origin: point, dir: lightDir };
  for (const s of scene.spheres) {
    const h = intersectSphere(ray, s);
    if (h && h.t < lightDist) return true;
  }
  for (const p of scene.planes) {
    const h = intersectPlane(ray, p);
    if (h && h.t < lightDist) return true;
  }
  return false;
}

function shade(ray, scene, depth = 0) {
  if (depth > 5) return new Vec3(0.05, 0.05, 0.1);

  const hit = traceScene(ray, scene);
  if (!hit) {
    const t = 0.5 * (ray.dir.normalized().y + 1.0);
    return new Vec3(0.02, 0.02, 0.05).mul(1.0 - t).add(new Vec3(0.1, 0.1, 0.2).mul(t));
  }

  const mat = hit.material;
  const ambient = new Vec3(0.05, 0.05, 0.1);
  let color = mat.color.mul(ambient).mul(mat.ambient);

  for (const light of scene.lights) {
    const toLight = light.position.sub(hit.point);
    const lightDist = toLight.length();
    const lightDir = toLight.normalized();

    if (inShadow(hit.point, lightDir, lightDist, scene)) continue;

    const diff = Math.max(0, hit.normal.dot(lightDir));
    color = color.add(mat.color.mul(light.color).mul(diff * mat.diffuse * light.intensity));

    const viewDir = ray.origin.sub(hit.point).normalized();
    const halfDir = lightDir.add(viewDir).normalized();
    const spec = Math.pow(Math.max(0, hit.normal.dot(halfDir)), mat.shininess);
    color = color.add(light.color.mul(spec * mat.specular * light.intensity));
  }

  if (mat.reflectivity > 0 && depth < 5) {
    const reflDir = ray.dir.normalized().reflect(hit.normal);
    const reflRay = { origin: hit.point.add(hit.normal.mul(0.001)), dir: reflDir };
    const reflColor = shade(reflRay, scene, depth + 1);
    color = color.mul(1.0 - mat.reflectivity).add(reflColor.mul(mat.reflectivity));
  }

  return color;
}

function tonemap(v) {
  v = v / (v + 1.0);
  v = Math.pow(v, 1.0 / 2.2);
  return Math.min(Math.max(Math.round(v * 255), 0), 255);
}

// Deserialize scene (plain objects -> Vec3 instances)
function deserializeScene(data) {
  const v = (o) => new Vec3(o.x, o.y, o.z);
  return {
    spheres: data.spheres.map(s => ({
      center: v(s.center),
      radius: s.radius,
      enabled: s.enabled,
      material: { ...s.material, color: v(s.material.color) },
    })),
    planes: data.planes.map(p => ({
      point: v(p.point),
      normal: v(p.normal),
      scale: p.scale,
      mat1: { ...p.mat1, color: v(p.mat1.color) },
      mat2: { ...p.mat2, color: v(p.mat2.color) },
    })),
    lights: data.lights.map(l => ({
      position: v(l.position),
      color: v(l.color),
      intensity: l.intensity,
    })),
    camPos: v(data.camPos),
    camTarget: v(data.camTarget),
    fov: data.fov,
  };
}

// ─── Render handler ───────────────────────────────────────
self.onmessage = function(e) {
  const { type, scene: sceneData, width, height, samples } = e.data;
  if (type !== 'render') return;

  const scene = deserializeScene(sceneData);

  const camUp = new Vec3(0, 1, 0);
  const forward = scene.camTarget.sub(scene.camPos).normalized();
  const right = forward.cross(camUp).normalized();
  const up = right.cross(forward);
  const aspect = width / height;
  const fovRad = scene.fov * Math.PI / 180;
  const halfH = Math.tan(fovRad / 2);
  const halfW = halfH * aspect;

  const pixels = new Uint8ClampedArray(width * height * 4);
  const BATCH = 8; // send progress every N rows

  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      let cr = 0, cg = 0, cb = 0;

      for (let sy = 0; sy < samples; sy++) {
        for (let sx = 0; sx < samples; sx++) {
          const u = ((x + (sx + 0.5) / samples) / width) * 2.0 - 1.0;
          const v = ((y + (sy + 0.5) / samples) / height) * 2.0 - 1.0;
          const dir = forward.add(right.mul(u * halfW)).add(up.mul(v * halfH)).normalized();
          const ray = { origin: scene.camPos, dir };
          const color = shade(ray, scene);
          cr += color.x;
          cg += color.y;
          cb += color.z;
        }
      }

      const invS = 1.0 / (samples * samples);
      const idx = (y * width + x) * 4;
      pixels[idx + 0] = tonemap(cr * invS);
      pixels[idx + 1] = tonemap(cg * invS);
      pixels[idx + 2] = tonemap(cb * invS);
      pixels[idx + 3] = 255;
    }

    if (y % BATCH === 0 || y === height - 1) {
      self.postMessage({
        type: 'progress',
        row: y,
        total: height,
        pixels: pixels.buffer,
      }, [pixels.buffer.slice(0)]); // send copy
    }
  }

  self.postMessage({
    type: 'done',
    pixels: pixels.buffer,
    width,
    height,
  }, [pixels.buffer]);
};
