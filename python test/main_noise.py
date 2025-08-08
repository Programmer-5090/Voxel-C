import pygame
import sys
from typing import List
import opensimplex

def map_value(value, from_range, to_range):
    from_min, from_max = from_range
    to_min, to_max = to_range
    # Clamp the value to the from_range
    value = max(from_min, min(from_max, value))
    return to_min + (value - from_min) * (to_max - to_min) / (from_max - from_min)

class terrainCell:
    def __init__(self):
        self.altitude = 0.0
        self.color = (0, 0, 0)
    
    def calcColor(self):
        # Clamp altitude to reasonable range
        altitude = max(-1.0, min(1.0, self.altitude))
        if altitude < -0.3:
            # Deep water (dark blue)
            self.color = (0, 50, 150)
        elif altitude < 0:
            # Shallow water (light blue)
            blue_val = int(map_value(altitude, [-0.3, 0], [150, 200]))
            self.color = (0, 100, blue_val)
        elif altitude < 0.1:
            # Beach/sand (yellow-brown)
            self.color = (200, 180, 100)
        elif altitude < 0.3:
            # Grass/plains (green)
            green_val = int(map_value(altitude, [0.1, 0.3], [120, 180]))
            self.color = (50, green_val, 50)
        elif altitude < 0.6:
            # Hills/forest (dark green)
            green_val = int(map_value(altitude, [0.3, 0.6], [100, 150]))
            self.color = (30, green_val, 30)
        elif altitude < 0.7:
            # Mountains (brown/gray)
            brown_val = int(map_value(altitude, [0.6, 0.7], [80, 120]))
            self.color = (brown_val, brown_val - 20, brown_val - 40)
        else:
            # More snow on mountains (lowered threshold)
            snow_val = int(map_value(altitude, [0.7, 1.0], [200, 255]))
            self.color = (snow_val, snow_val, snow_val)
        # Ensure all color values are in valid range [0, 255]
        self.color = tuple(max(0, min(255, val)) for val in self.color)
            
    def display(self, screen, x: int, y: int):
        rect = pygame.Rect(x, y, scale, scale)
        pygame.draw.rect(screen, self.color, rect, border_radius=0)

terrain: List[List[terrainCell]] = []
scale: int = 2  # Increased for better performance
noiseScale: float = 0.01  # Reduced for larger features
mapHeight : int = 0
mapWidth : int = 0

# Camera/viewport variables
camera_x: float = 0.0
camera_y: float = 0.0
dragging: bool = False
last_mouse_pos = (0, 0)

def generate_terrain_height(x, y, continent_mode=True):
    """Generate terrain height using multiple octaves of noise for more realistic mountains"""
    height = 0.0
    if continent_mode:
        # Large scale features (continents)
        height += opensimplex.noise2(x * 0.005, y * 0.005) * 0.8
    else:
        # Continuous terrain: no large scale, just mountains everywhere
        height += opensimplex.noise2(x * 0.02, y * 0.02) * 0.8
    # Medium scale features (hills and valleys)
    height += opensimplex.noise2(x * 0.02, y * 0.02) * 0.3
    # Small scale features (details)
    height += opensimplex.noise2(x * 0.08, y * 0.08) * 0.1
    # Add some ridges for mountain-like features
    ridge = abs(opensimplex.noise2(x * 0.01, y * 0.01)) * 0.4
    height += ridge
    # Normalize to -1 to 1 range but favor positive values for more landmass
    if continent_mode:
        height = height * 0.7 + 0.2  # Shift up to create more land
    else:
        height = height * 0.7  # No shift, more continuous
    return max(-1.0, min(1.0, height))

def get_terrain_at(world_x, world_y, continent_mode=True):
    """Get terrain cell at world coordinates, generating it if needed"""
    cell = terrainCell()
    cell.altitude = generate_terrain_height(world_x, world_y, continent_mode)
    cell.calcColor()
    return cell

# Pygame window setup
pygame.init()
width: int = 800
height: int = 800
screen = pygame.display.set_mode((width, height))
clock = pygame.time.Clock()
def set_up():
    global mapWidth, mapHeight
    mapWidth = int(width/scale)
    mapHeight = int(height/scale)
    
    # No need to pre-generate terrain - we'll generate it dynamically

def main():
    global camera_x, camera_y, dragging, last_mouse_pos
    set_up()
    running = True
    continent_mode = True
    while running:
        # Handle events
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    running = False
                elif event.key == pygame.K_TAB:
                    continent_mode = not continent_mode
            elif event.type == pygame.MOUSEBUTTONDOWN:
                if event.button == 1:  # Left mouse button
                    dragging = True
                    last_mouse_pos = pygame.mouse.get_pos()
            elif event.type == pygame.MOUSEBUTTONUP:
                if event.button == 1:  # Left mouse button
                    dragging = False
            elif event.type == pygame.MOUSEMOTION:
                if dragging:
                    current_mouse_pos = pygame.mouse.get_pos()
                    dx = current_mouse_pos[0] - last_mouse_pos[0]
                    dy = current_mouse_pos[1] - last_mouse_pos[1]
                    # Move camera (invert for natural dragging feel)
                    camera_x -= dx / scale
                    camera_y -= dy / scale
                    last_mouse_pos = current_mouse_pos
        # Clear screen with a background color
        screen.fill((20, 20, 40))  # Dark blue background
        # Draw terrain based on camera position
        for screen_y in range(0, height, scale):
            for screen_x in range(0, width, scale):
                # Convert screen coordinates to world coordinates
                world_x = int(camera_x + screen_x / scale)
                world_y = int(camera_y + screen_y / scale)
                # Get terrain at this world position
                terrain_cell = get_terrain_at(world_x, world_y, continent_mode)
                # Draw the cell
                rect = pygame.Rect(screen_x, screen_y, scale, scale)
                pygame.draw.rect(screen, terrain_cell.color, rect)
        # Draw UI instructions
        font = pygame.font.Font(None, 36)
        text = font.render("Click and drag to explore the world!", True, (255, 255, 255))
        screen.blit(text, (10, 10))
        mode_text = font.render(f"Mode: {'Continent' if continent_mode else 'Continuous'} (TAB to switch)", True, (255, 255, 0))
        screen.blit(mode_text, (10, 50))
        coords_text = font.render(f"Position: ({int(camera_x)}, {int(camera_y)})", True, (255, 255, 255))
        screen.blit(coords_text, (10, 90))
        # Update display
        pygame.display.flip()
        clock.tick(60)  # 60 FPS
    pygame.quit()
    sys.exit()

if __name__ == "__main__":
    main()
