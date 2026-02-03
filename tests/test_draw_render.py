"""Tests for the pixel-art circuit renderer (draw.py).

Uses synthetic draw_data dicts to avoid Cython build dependency.
"""

import numpy as np

from quantum_language.draw import (
    BG_COLOR,
    CELL_H,
    CELL_W,
    GATE_COLORS,
    WIRE_COLOR,
    render,
)


def make_draw_data(num_layers, num_qubits, gates=None):
    """Build a synthetic draw_data dict."""
    return {
        "num_layers": num_layers,
        "num_qubits": num_qubits,
        "gates": gates or [],
        "qubit_map": {},
    }


def make_gate(layer, target, gate_type, angle=0.0, controls=None):
    """Build a synthetic gate dict."""
    return {
        "layer": layer,
        "target": target,
        "type": gate_type,
        "angle": angle,
        "controls": controls or [],
    }


class TestEmptyCircuit:
    def test_empty_circuit(self):
        """Empty circuit (0 layers, 0 qubits) returns a 1x1 image."""
        dd = make_draw_data(0, 0)
        img = render(dd)
        assert img.size == (1, 1)
        arr = np.array(img)
        np.testing.assert_array_equal(arr[0, 0], BG_COLOR)


class TestCanvasDimensions:
    def test_canvas_dimensions(self):
        """Canvas size matches num_layers * CELL_W x num_qubits * CELL_H."""
        dd = make_draw_data(5, 3)
        img = render(dd)
        expected_w = 5 * CELL_W  # 15
        expected_h = 3 * CELL_H  # 9
        assert img.size == (expected_w, expected_h)


class TestBackgroundColor:
    def test_background_color(self):
        """Most pixels in a gate-free circuit are BG_COLOR."""
        dd = make_draw_data(5, 3)
        img = render(dd)
        arr = np.array(img)
        bg = np.array(BG_COLOR, dtype=np.uint8)
        bg_mask = np.all(arr == bg, axis=-1)
        # With 3 wires (1px each across 15 cols = 45 wire pixels)
        # Total pixels: 15*9 = 135; wire pixels = 45; BG = 90
        assert bg_mask.sum() > 0.5 * arr.shape[0] * arr.shape[1]


class TestWireRendering:
    def test_wire_rendering(self):
        """Wire lines appear at qubit row centers."""
        dd = make_draw_data(4, 2)
        img = render(dd)
        arr = np.array(img)
        wire_color = np.array(WIRE_COLOR, dtype=np.uint8)

        # Qubit 0 wire at y = CELL_H // 2 = 1
        wire_y0 = CELL_H // 2
        np.testing.assert_array_equal(arr[wire_y0, 0], wire_color)

        # Qubit 1 wire at y = CELL_H + CELL_H // 2 = 4
        wire_y1 = CELL_H + CELL_H // 2
        np.testing.assert_array_equal(arr[wire_y1, 0], wire_color)


class TestSingleQubitGatePosition:
    def test_single_qubit_gate_position(self):
        """H gate at layer=1, target=0 renders 2x2 block at correct position."""
        gate = make_gate(layer=1, target=0, gate_type=4)
        dd = make_draw_data(3, 2, [gate])
        img = render(dd)
        arr = np.array(img)
        expected = np.array(GATE_COLORS[4], dtype=np.uint8)

        gx = 1 * CELL_W  # 3
        gy = 0 * CELL_H  # 0

        # All 4 pixels of the 2x2 block should match
        np.testing.assert_array_equal(arr[gy, gx], expected)
        np.testing.assert_array_equal(arr[gy, gx + 1], expected)
        np.testing.assert_array_equal(arr[gy + 1, gx], expected)
        np.testing.assert_array_equal(arr[gy + 1, gx + 1], expected)


class TestAllGateTypesHaveColors:
    def test_all_gate_types_have_colors(self):
        """All 10 gate types (0-9) render with non-background color."""
        bg = np.array(BG_COLOR, dtype=np.uint8)
        gates = [make_gate(layer=i, target=0, gate_type=i) for i in range(10)]
        dd = make_draw_data(10, 1, gates)
        img = render(dd)
        arr = np.array(img)

        for gate_type in range(10):
            gx = gate_type * CELL_W
            gy = 0
            pixel = arr[gy, gx]
            # Gate pixel should NOT be background
            assert not np.array_equal(pixel, bg), (
                f"Gate type {gate_type} at ({gy},{gx}) is still BG_COLOR"
            )
            # Should match expected color
            expected = np.array(GATE_COLORS[gate_type], dtype=np.uint8)
            np.testing.assert_array_equal(pixel, expected)


class TestMeasurementCheckerboard:
    def test_measurement_checkerboard(self):
        """Measurement gate (type=9) renders checkerboard, not solid block."""
        gate = make_gate(layer=0, target=0, gate_type=9)
        dd = make_draw_data(1, 1, [gate])
        img = render(dd)
        arr = np.array(img)

        m_color = np.array(GATE_COLORS[9], dtype=np.uint8)
        bg = np.array(BG_COLOR, dtype=np.uint8)

        # Checkerboard: (0,0) and (1,1) are measurement color
        np.testing.assert_array_equal(arr[0, 0], m_color)
        np.testing.assert_array_equal(arr[1, 1], m_color)

        # (0,1) remains background; (1,0) may be wire color since
        # wire_y = CELL_H // 2 = 1 overlaps the gate area
        np.testing.assert_array_equal(arr[0, 1], bg)
        # Verify (1,0) is NOT measurement color (it's wire or bg)
        assert not np.array_equal(arr[1, 0], m_color)


class TestNumpyBulkOperations:
    def test_numpy_bulk_operations(self):
        """Render returns PIL Image in RGB mode with uint8 array."""
        dd = make_draw_data(2, 2)
        img = render(dd)

        assert img.mode == "RGB"
        arr = np.array(img)
        assert arr.dtype == np.uint8
        assert arr.shape == (2 * CELL_H, 2 * CELL_W, 3)
