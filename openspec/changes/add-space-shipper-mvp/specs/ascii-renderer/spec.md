# Spec Delta

## Purpose

Renders the game world as a true ASCII 3D framebuffer: a character-cell grid
with colors and per-cell depth, produced by a deterministic software
rasterizer that any platform blits.

## ADDED Requirements

### Requirement: Character-cell framebuffer
The renderer SHALL produce frames as a grid of at most 160x90 cells, where
each cell holds an ASCII glyph, an RGB332 foreground color, an RGB332
background color, and a depth value.

#### Scenario: Frame dimensions
- **WHEN** a frame is rendered
- **THEN** every row contains exactly the grid width in cells and there are exactly the grid height rows

#### Scenario: Out-of-bounds writes are safe
- **WHEN** text or glyphs are drawn outside the grid bounds
- **THEN** the write is ignored and no memory outside the grid is touched

### Requirement: First-person 3D rasterization
The renderer SHALL project a tile-based level from a camera with position,
yaw and pitch into the character grid, drawing walls, perspective floor and
ceiling rows, and billboard sprites with per-cell depth testing.

#### Scenario: Wall visible ahead
- **WHEN** the camera stands on open floor facing a wall a few tiles away
- **THEN** the center screen columns show non-space glyphs where the wall projects

#### Scenario: Sprite occlusion
- **WHEN** a sprite is farther than a wall along the same screen column
- **THEN** the wall glyph remains and the sprite does not paint that column

#### Scenario: Deterministic output
- **WHEN** the same level and camera are rendered twice
- **THEN** both frames are identical

### Requirement: Brightness glyph ramp
The renderer SHALL map shaded brightness to a fixed ten-level ASCII ramp so
that distance falloff and side shading are visible in the glyph itself.

#### Scenario: Ramp ordering
- **WHEN** the ramp is inspected
- **THEN** it is ordered from space (darkest) to '@' (brightest) with no duplicate glyphs

### Requirement: Auto-opening doors
Door tiles SHALL be treated as open floor for both rendering and collision
while the player is within 2.5 tiles, and as solid walls otherwise.

#### Scenario: Door near player opens
- **WHEN** the player is within 2.5 tiles of a door
- **THEN** the door tile renders and collides as floor

#### Scenario: Door far from player stays shut
- **WHEN** the player is more than 2.5 tiles from a door
- **THEN** the door tile renders and collides as a solid wall
