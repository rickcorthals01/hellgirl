# Blender: writes T_GraveNoise.png, a seamlessly tiling cloud noise (white noise shaped to 1/f^beta in the frequency
# domain, which wraps by construction). Used by the graveyard's mist and to break up the ground's tiling.
#   blender -b --factory-startup -P graveyard_noise.py -- <output.png>
import bpy, sys
import numpy as np

out = sys.argv[sys.argv.index("--") + 1]
N = 512
rng = np.random.default_rng(1966)
fx = np.fft.fftfreq(N)[:, None]
fy = np.fft.fftfreq(N)[None, :]
f = np.sqrt(fx * fx + fy * fy)
f[0, 0] = 1.0
spectrum = np.fft.fft2(rng.standard_normal((N, N))) / f ** 1.8
spectrum[f < 3.0 / N] = 0  # no blob bigger than a third of the tile, so the repeats do not show
field = np.real(np.fft.ifft2(spectrum))
field = (field - field.min()) / (field.max() - field.min())
# Soft contrast: wispy clouds with clear gaps.
field = np.clip((field - .35) / .45, 0, 1) ** 1.3
image = bpy.data.images.new("T_GraveNoise", N, N, alpha=False, float_buffer=False)
rgba = np.ones((N, N, 4), dtype=np.float32)
rgba[..., 0] = rgba[..., 1] = rgba[..., 2] = field
image.pixels.foreach_set(rgba.ravel())
image.filepath_raw = out
image.file_format = "PNG"
image.save()
print("GRAVE NOISE DONE", float(field.mean()))
