// SPDX-License-Identifier: Apache-2.0
/*
 * The landing page's 3D viewport: a scalp mesh and its strands, drawn with three.js. Data-driven:
 * it takes StrandData and a RootMesh from whatever makes them (the procedural groom today, the WASM
 * engine later) and only draws. Strands are GL lines shaded as hair (Kajiya-Kay highlights, the
 * hair volume's own shadow, darker roots); the scalp is soft studio lit. A slow turntable, drag to
 * orbit (touch drags turn it sideways only, so the page still scrolls), and it only renders while
 * on screen.
 */
import {
  BufferAttribute,
  BufferGeometry,
  Color,
  DirectionalLight,
  DynamicDrawUsage,
  Group,
  HemisphereLight,
  LineBasicMaterial,
  LineSegments,
  Mesh,
  MeshBasicMaterial,
  MeshLambertMaterial,
  PerspectiveCamera,
  Points,
  Scene,
  ShaderMaterial,
  SRGBColorSpace,
  Vector3,
  WebGLRenderer,
} from 'three';
import type { RootMesh, StrandData } from './strand-data';

export interface ViewerOptions {
  /** prefers-reduced-motion: no turntable, no sway, no grow; one frame per change. */
  still: boolean;
  /** Turntable speed, radians a second. */
  spin?: number;
  /** Tip sway, 0 to 1. */
  sway?: number;
  /** Camera elevation, radians. */
  pitch?: number;
  /** How much of the view the groom fills (1 = its bounding sphere touches the edges). */
  fill?: number;
  /** Called before each frame with seconds since start, for a source that changes over time. */
  onFrame?: (time: number) => void;
}

export interface StrandViewer {
  setScalp(mesh: RootMesh): void;
  setStrands(data: StrandData): void;
  /** New colors for the strands already set (same layout), without touching their shape. */
  setStrandColors(colors: Float32Array): void;
  setGuides(data: StrandData | null): void;
  /** Frames the camera on this sphere (else on the scalp's bounds). */
  setFraming(center: [number, number, number], radius: number): void;
  /** Grows the strands in from their roots over `seconds`. */
  grow(seconds: number): void;
  setActive(active: boolean): void;
  resize(width: number, height: number): void;
  invalidate(): void;
  dispose(): void;
}

const hairVertex = /* glsl */ `
attribute vec3 tang;
attribute vec3 hairColor;
attribute vec2 meta;
uniform float uTime;
uniform float uSway;
uniform vec3 uCenter;
varying vec3 vColor;
varying vec3 vT;
varying vec3 vN;
varying vec3 vP;
varying float vS;
void main() {
  vec3 p = position;
  float s = meta.x;
  p += uSway * s * s * 0.035 * vec3(sin(uTime * 1.3 + meta.y), 0.35 * sin(uTime * 1.9 + meta.y * 2.0), cos(uTime * 1.0 + meta.y * 1.3));
  vec4 mv = modelViewMatrix * vec4(p, 1.0);
  vT = normalize(normalMatrix * tang);
  vN = normalize(normalMatrix * (p - uCenter));
  vP = mv.xyz;
  vColor = hairColor;
  vS = s;
  gl_Position = projectionMatrix * mv;
}
`;

const hairFragment = /* glsl */ `
uniform vec3 uKey;
uniform vec3 uFill;
uniform float uGrow;
varying vec3 vColor;
varying vec3 vT;
varying vec3 vN;
varying vec3 vP;
varying float vS;
vec3 toLinear(vec3 c) {
  return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(0.04045, c));
}
void main() {
  if (vS > uGrow) discard;
  vec3 base = toLinear(vColor);
  vec3 T = normalize(vT);
  vec3 N = normalize(vN);
  vec3 V = normalize(-vP);
  // The hair volume shades itself: strands on the far side of the key light sit in shadow.
  float lit = smoothstep(-0.3, 0.7, dot(N, uKey));
  float TL = dot(T, uKey);
  float diffuse = sqrt(max(0.0, 1.0 - TL * TL));
  vec3 H = normalize(uKey + V);
  float TH = dot(T, H);
  float sinTH = sqrt(max(0.0, 1.0 - TH * TH));
  float spec = pow(sinTH, 140.0) * 0.28 + pow(sinTH, 22.0) * 0.18;
  float fill = 0.5 + 0.5 * dot(N, uFill);
  float ao = mix(0.5, 1.0, smoothstep(0.0, 0.5, vS));
  float sky = 0.62 + 0.38 * N.y;
  vec3 col = base * ao * (0.42 * sky + 0.2 * fill + 0.95 * diffuse * lit);
  col += mix(vec3(1.0), base * 2.2, 0.45) * spec * lit * ao;
  float rim = pow(1.0 - abs(dot(N, V)), 3.0);
  col += base * rim * 0.35;
  gl_FragColor = vec4(col, 1.0);
  #include <colorspace_fragment>
}
`;

const dotVertex = /* glsl */ `
uniform float uSize;
void main() {
  gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
  gl_PointSize = uSize;
}
`;

const dotFragment = /* glsl */ `
uniform vec3 uRing;
void main() {
  vec2 c = gl_PointCoord * 2.0 - 1.0;
  float r = length(c);
  if (r > 1.0) discard;
  gl_FragColor = vec4(r > 0.5 ? uRing : vec3(1.0), 1.0);
  #include <colorspace_fragment>
}
`;

export function createStrandViewer(canvas: HTMLCanvasElement, opts: ViewerOptions): StrandViewer | null {
  let renderer: WebGLRenderer;
  try {
    renderer = new WebGLRenderer({ canvas, antialias: true, alpha: true, powerPreference: 'low-power' });
  } catch {
    return null;
  }
  renderer.setClearColor(0x000000, 0);
  renderer.autoClear = false;

  const still = opts.still;
  const spin = still ? 0 : (opts.spin ?? 0.22);
  const fov = 28;
  const camera = new PerspectiveCamera(fov, 1, 0.05, 100);
  const scene = new Scene();
  const overlay = new Scene();
  const group = new Group();
  const overlayGroup = new Group();
  scene.add(group);
  overlay.add(overlayGroup);

  // Soft studio light: a sky/ground fill and a key from the upper left, front.
  scene.add(new HemisphereLight(0xffffff, 0xbab3ad, 1.9));
  const keyDir = new Vector3(-2.2, 3.4, 3.2).normalize();
  const key = new DirectionalLight(0xffffff, 1.7);
  key.position.copy(keyDir);
  scene.add(key);
  const fillDir = new Vector3(3, 0.5, 1.5).normalize();
  const fillLight = new DirectionalLight(0xfff4ea, 0.45);
  fillLight.position.copy(fillDir);
  scene.add(fillLight);

  const scalpMaterial = new MeshLambertMaterial({ color: new Color('#dcd6d0') });
  let scalpColorKey = '';
  const depthMaterial = new MeshBasicMaterial({ colorWrite: false });
  let scalpGeometry: BufferGeometry | null = null;
  const scalpMesh = new Mesh(new BufferGeometry(), scalpMaterial);
  const scalpDepth = new Mesh(new BufferGeometry(), depthMaterial);
  group.add(scalpMesh);
  overlayGroup.add(scalpDepth);

  const hairMaterial = new ShaderMaterial({
    vertexShader: hairVertex,
    fragmentShader: hairFragment,
    uniforms: {
      uTime: { value: 0 },
      uSway: { value: 0 },
      uGrow: { value: 1 },
      uCenter: { value: new Vector3() },
      uKey: { value: new Vector3() },
      uFill: { value: new Vector3() },
    },
  });
  let hairGeometry = new BufferGeometry();
  const hair = new LineSegments(hairGeometry, hairMaterial);
  hair.frustumCulled = false;
  group.add(hair);
  let capacity = 0;
  let indexCapacity = 0;
  let layout = new Uint32Array(0);
  let layoutCount = -1;

  const guideLineMaterial = new LineBasicMaterial({ color: new Color('#3a5ea3') });
  const dotMaterial = new ShaderMaterial({
    vertexShader: dotVertex,
    fragmentShader: dotFragment,
    uniforms: { uSize: { value: 6 }, uRing: { value: new Color('#3a5ea3') } },
  });
  let guideLines: LineSegments | null = null;
  let guideDots: Points | null = null;

  const center = new Vector3();
  let radius = 1.6;
  let framed = false;
  let width = 1;
  let height = 1;
  let yaw = -0.5;
  let pitch = opts.pitch ?? 0.32;
  let yawVelocity = 0;
  let spinFactor = 1;
  let dragging = false;
  let active = false;
  let raf = 0;
  let inFrame = false;
  let growStart = -1;
  let growLength = 0;
  const start = performance.now();
  let last = start;

  const ensureCapacity = (points: number, segments: number) => {
    if (points <= capacity && segments * 2 <= indexCapacity) return;
    capacity = Math.max(points, Math.ceil(capacity * 1.25));
    indexCapacity = Math.max(segments * 2, Math.ceil(indexCapacity * 1.25));
    hairGeometry.dispose();
    hairGeometry = new BufferGeometry();
    const attr = (size: number) => new BufferAttribute(new Float32Array(capacity * size), size).setUsage(DynamicDrawUsage);
    hairGeometry.setAttribute('position', attr(3));
    hairGeometry.setAttribute('tang', attr(3));
    hairGeometry.setAttribute('hairColor', attr(3));
    hairGeometry.setAttribute('meta', attr(2));
    hairGeometry.setIndex(new BufferAttribute(new Uint32Array(indexCapacity), 1).setUsage(DynamicDrawUsage));
    hair.geometry = hairGeometry;
    layoutCount = -1;
  };

  const touch = (name: string, count: number) => {
    const a = hairGeometry.getAttribute(name) as BufferAttribute;
    a.clearUpdateRanges();
    a.addUpdateRange(0, count * a.itemSize);
    a.needsUpdate = true;
  };

  const viewer: StrandViewer = {
    setScalp(mesh) {
      scalpGeometry?.dispose();
      const g = new BufferGeometry();
      g.setAttribute('position', new BufferAttribute(mesh.positions, 3));
      if (mesh.normals) g.setAttribute('normal', new BufferAttribute(mesh.normals, 3));
      if (mesh.colors) {
        // Vertex colors are sRGB; the material works in linear.
        const c = new Color();
        const lin = new Float32Array(mesh.colors.length);
        for (let i = 0; i < lin.length; i += 3) {
          c.setRGB(mesh.colors[i], mesh.colors[i + 1], mesh.colors[i + 2], SRGBColorSpace);
          lin[i] = c.r;
          lin[i + 1] = c.g;
          lin[i + 2] = c.b;
        }
        g.setAttribute('color', new BufferAttribute(lin, 3));
      }
      const key = mesh.colors ? 'vertex' : 'flat';
      if (key !== scalpColorKey) {
        scalpColorKey = key;
        scalpMaterial.vertexColors = !!mesh.colors;
        scalpMaterial.color.set(mesh.colors ? '#ffffff' : '#dcd6d0');
        scalpMaterial.needsUpdate = true;
      }
      g.setIndex(new BufferAttribute(mesh.indices, 1));
      if (!mesh.normals) g.computeVertexNormals();
      g.computeBoundingSphere();
      scalpGeometry = g;
      scalpMesh.geometry = g;
      scalpDepth.geometry = g;
      if (!framed && g.boundingSphere) {
        center.copy(g.boundingSphere.center);
        radius = g.boundingSphere.radius * 1.45;
        hairMaterial.uniforms.uCenter.value.copy(center);
        frame();
      }
      viewer.invalidate();
    },

    setStrands(data) {
      const n = data.count;
      const points = data.offsets[n];
      const segments = points - n;
      ensureCapacity(points, segments);
      const pos = hairGeometry.getAttribute('position').array as Float32Array;
      const tang = hairGeometry.getAttribute('tang').array as Float32Array;
      const meta = hairGeometry.getAttribute('meta').array as Float32Array;
      pos.set(data.positions.subarray(0, points * 3));
      for (let i = 0; i < n; i++) {
        const a = data.offsets[i];
        const b = data.offsets[i + 1];
        const last = b - a - 1;
        // A phase for the sway from where the root is, so neighbors sway together.
        const phase = pos[a * 3] * 3.1 + pos[a * 3 + 1] * 1.7 + pos[a * 3 + 2] * 2.3;
        for (let p = a; p < b; p++) {
          const p0 = Math.max(a, p - 1) * 3;
          const p1 = Math.min(b - 1, p + 1) * 3;
          tang[p * 3] = pos[p1] - pos[p0];
          tang[p * 3 + 1] = pos[p1 + 1] - pos[p0 + 1];
          tang[p * 3 + 2] = pos[p1 + 2] - pos[p0 + 2];
          meta[p * 2] = last > 0 ? (p - a) / last : 0;
          meta[p * 2 + 1] = phase;
        }
      }
      touch('position', points);
      touch('tang', points);
      touch('meta', points);

      // The segment index, only when the strands' layout changed.
      let same = layoutCount === n;
      for (let i = 0; same && i <= n; i++) same = layout[i] === data.offsets[i];
      if (!same) {
        if (layout.length < n + 1) layout = new Uint32Array(n + 1);
        layout.set(data.offsets.subarray(0, n + 1));
        layoutCount = n;
        const index = hairGeometry.getIndex()!;
        const idx = index.array as Uint32Array;
        let k = 0;
        for (let i = 0; i < n; i++)
          for (let p = data.offsets[i]; p < data.offsets[i + 1] - 1; p++) {
            idx[k++] = p;
            idx[k++] = p + 1;
          }
        index.clearUpdateRanges();
        index.addUpdateRange(0, k);
        index.needsUpdate = true;
        hairGeometry.setDrawRange(0, k);
      }
      if (data.colors) viewer.setStrandColors(data.colors);
      else if (!same) viewer.setStrandColors(new Float32Array(points * 3).fill(0.4));
      viewer.invalidate();
    },

    setStrandColors(colors) {
      const points = layoutCount < 0 ? 0 : layout[layoutCount];
      const col = hairGeometry.getAttribute('hairColor').array as Float32Array;
      col.set(colors.subarray(0, points * 3));
      touch('hairColor', points);
      viewer.invalidate();
    },

    setGuides(data) {
      for (const o of [guideLines, guideDots]) {
        if (!o) continue;
        overlayGroup.remove(o);
        o.geometry.dispose();
      }
      guideLines = guideDots = null;
      if (data && data.count > 0) {
        const n = data.count;
        const idx: number[] = [];
        const dots: number[] = [];
        for (let i = 0; i < n; i++) {
          const a = data.offsets[i];
          const b = data.offsets[i + 1];
          for (let p = a; p < b - 1; p++) idx.push(p, p + 1);
          const step = Math.max(1, Math.round((b - a) / 6));
          for (let p = a; p < b; p += step) dots.push(data.positions[p * 3], data.positions[p * 3 + 1], data.positions[p * 3 + 2]);
        }
        const lines = new BufferGeometry();
        lines.setAttribute('position', new BufferAttribute(data.positions.slice(0, data.offsets[n] * 3), 3));
        lines.setIndex(idx);
        guideLines = new LineSegments(lines, guideLineMaterial);
        const points = new BufferGeometry();
        points.setAttribute('position', new BufferAttribute(new Float32Array(dots), 3));
        guideDots = new Points(points, dotMaterial);
        guideLines.frustumCulled = guideDots.frustumCulled = false;
        overlayGroup.add(guideLines, guideDots);
      }
      viewer.invalidate();
    },

    setFraming(c, r) {
      center.set(c[0], c[1], c[2]);
      radius = r;
      framed = true;
      hairMaterial.uniforms.uCenter.value.copy(center);
      frame();
      viewer.invalidate();
    },

    grow(seconds) {
      if (still) return;
      growStart = performance.now();
      growLength = seconds * 1000;
      hairMaterial.uniforms.uGrow.value = 0;
      viewer.invalidate();
    },

    setActive(on) {
      active = on;
      if (on) {
        last = performance.now();
        viewer.invalidate();
      } else if (raf) {
        cancelAnimationFrame(raf);
        raf = 0;
      }
    },

    resize(w, h) {
      width = Math.max(1, w);
      height = Math.max(1, h);
      const dpr = Math.min(window.devicePixelRatio || 1, 2);
      renderer.setPixelRatio(dpr);
      renderer.setSize(width, height, false);
      dotMaterial.uniforms.uSize.value = 6 * dpr;
      frame();
      viewer.invalidate();
    },

    invalidate() {
      if (!raf && active && !inFrame) raf = requestAnimationFrame(tick);
    },

    dispose() {
      active = false;
      cancelAnimationFrame(raf);
      canvas.removeEventListener('pointerdown', onDown);
      canvas.removeEventListener('pointermove', onMove);
      canvas.removeEventListener('pointerup', onUp);
      canvas.removeEventListener('pointercancel', onUp);
      viewer.setGuides(null);
      hairGeometry.dispose();
      scalpGeometry?.dispose();
      for (const m of [hairMaterial, scalpMaterial, depthMaterial, guideLineMaterial, dotMaterial]) m.dispose();
      renderer.dispose();
      renderer.forceContextLoss();
    },
  };

  /** Puts the camera where the groom's sphere fills the view, at the current elevation. */
  function frame() {
    camera.aspect = width / height;
    const v = (fov / 2) * (Math.PI / 180);
    const h = Math.atan(Math.tan(v) * camera.aspect);
    const dist = radius / Math.sin(Math.min(v, h)) / (opts.fill ?? 1);
    camera.position.set(center.x, center.y + Math.sin(pitch) * dist, center.z + Math.cos(pitch) * dist);
    camera.lookAt(center);
    camera.near = Math.max(0.01, dist - radius * 3);
    camera.far = dist + radius * 3;
    camera.updateProjectionMatrix();
  }

  const view = new Vector3();
  function tick(now: number) {
    raf = 0;
    if (!active) return;
    inFrame = true;
    const dt = Math.min(0.05, (now - last) / 1000);
    last = now;
    const t = (now - start) / 1000;
    opts.onFrame?.(t);
    inFrame = false;

    if (!dragging) {
      spinFactor = Math.min(1, spinFactor + dt * 0.6);
      yaw += spin * spinFactor * dt + yawVelocity * dt;
      yawVelocity *= Math.exp(-dt * 4);
      if (Math.abs(yawVelocity) < 0.01) yawVelocity = 0;
    }
    group.rotation.y = overlayGroup.rotation.y = yaw;
    frame();
    camera.updateMatrixWorld();
    const u = hairMaterial.uniforms;
    u.uTime.value = t;
    u.uSway.value = still ? 0 : (opts.sway ?? 0.6);
    u.uKey.value.copy(view.copy(keyDir).transformDirection(camera.matrixWorldInverse));
    u.uFill.value.copy(view.copy(fillDir).transformDirection(camera.matrixWorldInverse));
    let growing = false;
    if (growStart >= 0) {
      const k = Math.min(1, (now - growStart) / growLength);
      u.uGrow.value = 1 - Math.pow(1 - k, 3);
      growing = k < 1;
      if (!growing) growStart = -1;
    }

    renderer.clear();
    renderer.render(scene, camera);
    if (guideLines) {
      renderer.clearDepth();
      renderer.render(overlay, camera);
    }
    const moving = !still || dragging || yawVelocity !== 0 || growing;
    if (moving && active && !document.hidden) raf = requestAnimationFrame(tick);
  }

  // Drag to orbit. Touch turns it sideways only (touch-action: pan-y leaves vertical swipes to
  // the page); a mouse can also tip it up and down. Wheel and pinch are left alone.
  let lastX = 0;
  let lastY = 0;
  let lastT = 0;
  let pointer = -1;
  function onDown(e: PointerEvent) {
    if (pointer >= 0 || (e.pointerType === 'mouse' && e.button !== 0)) return;
    pointer = e.pointerId;
    dragging = true;
    yawVelocity = 0;
    lastX = e.clientX;
    lastY = e.clientY;
    lastT = e.timeStamp;
    canvas.setPointerCapture(e.pointerId);
    canvas.style.cursor = 'grabbing';
    viewer.invalidate();
  }
  function onMove(e: PointerEvent) {
    if (e.pointerId !== pointer) return;
    const dx = e.clientX - lastX;
    const dy = e.clientY - lastY;
    const dts = Math.max(1, e.timeStamp - lastT) / 1000;
    lastX = e.clientX;
    lastY = e.clientY;
    lastT = e.timeStamp;
    const k = 5.5 / Math.max(240, width);
    yaw += dx * k;
    yawVelocity = yawVelocity * 0.6 + ((dx * k) / dts) * 0.4;
    if (e.pointerType === 'mouse') pitch = Math.max(-0.15, Math.min(1.1, pitch + dy * k * 0.6));
    viewer.invalidate();
  }
  function onUp(e: PointerEvent) {
    if (e.pointerId !== pointer) return;
    pointer = -1;
    dragging = false;
    spinFactor = 0;
    if (e.type === 'pointercancel') yawVelocity = 0;
    yawVelocity = Math.max(-4, Math.min(4, yawVelocity));
    canvas.style.cursor = '';
    viewer.invalidate();
  }
  canvas.addEventListener('pointerdown', onDown);
  canvas.addEventListener('pointermove', onMove);
  canvas.addEventListener('pointerup', onUp);
  canvas.addEventListener('pointercancel', onUp);

  return viewer;
}
