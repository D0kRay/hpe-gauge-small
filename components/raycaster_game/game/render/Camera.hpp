#pragma once

#include "../Player.hpp"
#include "Screen.hpp"
#include "textures.h"
#include "Sprite.hpp"
#include "../map.h"

//constexpr uint16_t AO_Levels[4] = {0xffff, 0x8410, 0x4a69, 0x18c3};
constexpr float AO_Levels[4] = {1.0, 0.5, 0.3, 0.1};

#define FOG_ACTIVE 0

inline uint16_t blend(uint16_t c1, uint16_t c2, uint8_t alpha) {
    if (alpha == 0u) {
        return c1;
    }
    if (alpha >= 255u) {
        return c2;
    }

    const uint16_t c1_r = (c1 >> 11) & 0x1F;
    const uint16_t c1_g = (c1 >> 5) & 0x3F;
    const uint16_t c1_b = c1 & 0x1F;

    const uint16_t c2_r = (c2 >> 11) & 0x1F;
    const uint16_t c2_g = (c2 >> 5) & 0x3F;
    const uint16_t c2_b = c2 & 0x1F;

    const uint16_t inv_alpha = 255u - alpha;
    const uint16_t r = (uint16_t)((uint32_t)(c1_r * inv_alpha + c2_r * alpha) / 255u);
    const uint16_t g = (uint16_t)((uint32_t)(c1_g * inv_alpha + c2_g * alpha) / 255u);
    const uint16_t b = (uint16_t)((uint32_t)(c1_b * inv_alpha + c2_b * alpha) / 255u);

    return (uint16_t)((r << 11) | (g << 5) | b);
}

inline uint16_t smoothDarken(uint16_t c, float alpha) {
    float r = (c >> 11) & 0x1f;
    float g = (c >> 5) & 0x3f;
    float b = c & 0x1f;

    r = r*alpha;
    g = g*alpha;
    b = b*alpha;

    return uint16_t( (int(r) << 11) | (int(g) << 5) | int(b) );
}

static inline uint16_t applyFog(uint16_t c, float dist) {
    if (!FOG_ACTIVE) {
        return c;
    }

    const uint8_t alpha = (uint8_t)SDL_clamp(255.0f / fmaxf(dist, 1.0f), 0.0f, 255.0f);
    return blend(c, fogColor, alpha);
}

inline float interpolate(float c[4], float x, float y) {
    return c[0]*(1.0-x)*(1.0-y) + c[1]*x*(1.0-y) + c[2]*(1.0-x)*y + c[3]*x*y;
}

static inline int clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

class Camera {
private:
    static constexpr int H = Screen::SCREEN_HEIGHT;
    static constexpr int W = Screen::SCREEN_WIDTH;
    static constexpr int H_2 = H/2;
    static constexpr int W_2 = W/2;

    Vector2 pos;
    Vector2 dir;
    Vector2 plane;
public:
    float Zbuffer[W];

    void update(const Player& player) {
        pos = player.pos;
        dir = player.dir;
        plane = Vector2(dir.y, -dir.x);
    }

    void draw() {
        drawWalls();
        drawSprites();
    }

private:
    void drawWalls();
    void drawSprites();

    float getAlpha(int y, float perpDistance, float floorXWall, float floorYWall);
};

Camera camera;

void Camera::drawSprites() {
    player.spriteAtCenter = -1;
    SpriteManager::sortSprites(pos);

    for (int i = 0; i < SpriteManager::sprite_num; ++i) {
        Sprite sprite = SpriteManager::sprites[i];

        float spriteX = sprite.x - pos.x;
        float spriteY = sprite.y - pos.y;

        const float det = plane.x * dir.y - dir.x * plane.y;
        if (fabsf(det) < 0.0001f) {
            continue;
        }

        float invDet = 1.f / det;
        float transformX = invDet * (dir.y * spriteX - dir.x * spriteY);
        float transformY = invDet * (-plane.y * spriteX + plane.x * spriteY);

        if (!isfinite(transformY) || transformY <= 0.0001f) {
            continue;
        }

        int spriteHW = abs(int(H / transformY)) / sprite.scale;
        if (spriteHW <= 0) {
            continue;
        }

        int spriteScreenX = int(W_2 * (1.f + transformX / transformY));
        int spriteScreenZ = int(sprite.z / transformY);

        int drawStartY = -spriteHW / 2 + H_2 + spriteScreenZ;
        int drawEndY = spriteHW / 2 + H_2 + spriteScreenZ;
        int drawStartX = -spriteHW / 2 + spriteScreenX;
        int drawEndX = spriteHW / 2 + spriteScreenX;

        if (drawEndY <= 0 || drawStartY >= H || drawEndX < 0 || drawStartX >= W) {
            continue;
        }

        if (drawStartY < 0) drawStartY = 0;
        if (drawEndY >= H) drawEndY = H - 1;
        if (drawStartX < 0) drawStartX = 0;
        if (drawEndX >= W) drawEndX = W - 1;
        if (drawEndX < drawStartX || drawEndY < drawStartY) {
            continue;
        }

        for (int x = drawStartX; x <= drawEndX; ++x) {
            if (x < 0 || x >= W) {
                continue;
            }

            int texX = int(256 * (x - (-spriteHW / 2 + spriteScreenX)) * SPR_WIDTH / spriteHW) / 256;
            texX = clamp_int(texX, 0, SPR_WIDTH - 1);

            if (transformY < Zbuffer[x]) {
                uint8_t blendDist = 255 / fmaxf(1.f, transformY);

                for (int y = drawStartY; y <= drawEndY; ++y) {
                    if (y < 0 || y >= H) {
                        continue;
                    }

                    int d = (y - spriteScreenZ) * 256 - H * 128 + spriteHW * 128;
                    int texY = ((d * SPR_HEIGHT) / spriteHW) / 256;
                    if (texY < 0 || texY >= SPR_HEIGHT) {
                        continue;
                    }

                    uint16_t color = sprite.texture[SPR_WIDTH * texY + texX];
                    if (color != TRANSP) {
                        if (x == W_2) {
                            int idx = sprite.entity_idx;
                            if (EntityManager::entities[idx] != nullptr && !EntityManager::entities[idx]->isDead()) {
                                player.spriteAtCenter = idx;
                            }
                        }
                        if (FOG_ACTIVE) {
                            Screen::drawPixel(x, y, blend(color, fogColor, blendDist));
                        } else {
                            Screen::drawPixel(x, y, color);
                        }
                    }
                }
            }
        }
    }
}

float Camera::getAlpha(int y, float perpDistance, float floorXWall, float floorYWall) {
    if (!gameMap) {
        return 1.0f;
    }

    float currentDist = H / (2.0 * y - H);
    float weight = currentDist / perpDistance;

    float currentFloorX = weight * floorXWall + (1.0 - weight) * pos.x;
    float currentFloorY = weight * floorYWall + (1.0 - weight) * pos.y;

    int cellX = int(currentFloorX);
    int cellY = int(currentFloorY);

    if (cellX < 0) cellX = 0;
    else if (cellX >= MAP_WIDTH) cellX = MAP_WIDTH - 1;
    if (cellY < 0) cellY = 0;
    else if (cellY >= MAP_HEIGHT) cellY = MAP_HEIGHT - 1;

    int tx = int(float(FLOOR_WIDTH) * currentFloorX) & (FLOOR_WIDTH - 1);
    int ty = int(float(FLOOR_HEIGHT) * currentFloorY) & (FLOOR_HEIGHT - 1);

    int left = (cellX > 0) ? cellX - 1 : 0;
    int right = (cellX + 1 < MAP_WIDTH) ? cellX + 1 : MAP_WIDTH - 1;
    int top = (cellY > 0) ? cellY - 1 : 0;
    int bottom = (cellY + 1 < MAP_HEIGHT) ? cellY + 1 : MAP_HEIGHT - 1;

    int aoVertex[4] = {
        !!(gameMap[top][left]) + !!(gameMap[top][cellX]) + !!(gameMap[cellY][left]), //Top Left
        !!(gameMap[top][cellX]) + !!(gameMap[top][right]) + !!(gameMap[cellY][right]), //Top Right
        !!(gameMap[cellY][left]) + !!(gameMap[bottom][left]) + !!(gameMap[bottom][cellX]), //Bottom Left
        !!(gameMap[cellY][right]) + !!(gameMap[bottom][right]) + !!(gameMap[bottom][cellX]), //Bottom Right
    };

    float aoColors[4];
    for(int i=0; i<4; ++i)
        aoColors[i] = AO_Levels[aoVertex[i]];

    float xPer = float(tx)/(FLOOR_WIDTH-1), yPer = float(ty)/(FLOOR_HEIGHT-1);
    return interpolate(aoColors, xPer, yPer);
}

void Camera::drawWalls() {
    for(int x=0; x<W; ++x) {
        float camX = 2.0f * x / float(W) - 1.0f; // -1 to 1 being 0 the center of the screen
        float rayDirX = dir.x + plane.x*camX;
        float rayDirY = dir.y + plane.y*camX;

        if (fabsf(rayDirX) < 0.000001f && fabsf(rayDirY) < 0.000001f) {
            rayDirX = 1.0f;
            rayDirY = 0.0f;
        }

        if (fabsf(rayDirX) < 0.000001f) {
            rayDirX = copysignf(0.000001f, rayDirX == 0.0f ? 1.0f : rayDirX);
        }
        if (fabsf(rayDirY) < 0.000001f) {
            rayDirY = copysignf(0.000001f, rayDirY == 0.0f ? 1.0f : rayDirY);
        }

        int mapX = int(pos.x);
        int mapY = int(pos.y);

        float deltaDistX = fabsf(1.0f / rayDirX);
        float deltaDistY = fabsf(1.0f / rayDirY);

        float sideDistX, sideDistY;
        int stepX, stepY;

        if (rayDirX < 0) {
            stepX = -1;
            sideDistX = (pos.x - mapX) * deltaDistX;
        } else {
            stepX = 1;
            sideDistX = (mapX + 1.0f - pos.x) * deltaDistX;
        }

        if (rayDirY < 0) {
            stepY = -1;
            sideDistY = (pos.y - mapY) * deltaDistY;
        } else {
            stepY = 1;
            sideDistY = (mapY + 1.0f - pos.y) * deltaDistY;
        }

        int side;
        bool hit = false;
        while(!hit) {
            if(sideDistX < sideDistY) {
                sideDistX += deltaDistX;
                mapX += stepX;
                side = 0;
            } else {
                sideDistY += deltaDistY;
                mapY += stepY;
                side = 1;
            }

            if (mapX < 0) mapX = 0;
            else if (mapX >= MAP_WIDTH) mapX = MAP_WIDTH - 1;
            if (mapY < 0) mapY = 0;
            else if (mapY >= MAP_HEIGHT) mapY = MAP_HEIGHT - 1;

            hit = gameMap[mapY][mapX] > 0;
        }

        //Draw line code
        float perpDistance = side==0? (sideDistX - deltaDistX):(sideDistY - deltaDistY); //Perpendicular distance, not euclidean
        if (!isfinite(perpDistance) || perpDistance <= 0.0001f) {
            perpDistance = 0.0001f;
        }

        uint8_t blendDist = 255/fmaxf(1.f, perpDistance);

        int lineHeight = int(H / perpDistance);
        int drawStart = -lineHeight / 2 + H_2;
        if(drawStart < 0) drawStart = 0;
        int drawEnd = lineHeight / 2 + H_2;
        if(drawEnd >= H) drawEnd = H - 1;

        float wallX = side==0? (pos.y + perpDistance * rayDirY) : (pos.x + perpDistance * rayDirX);
        wallX -= floor(wallX); //From 0 to 1;

        int texX = int(wallX * float(TEX_WIDTH)) % TEX_WIDTH;
        if(side == 0 && rayDirX > 0.0f) texX = TEX_WIDTH - texX - 1;
        if(side == 1 && rayDirY < 0.0f) texX = TEX_WIDTH - texX - 1;

        float step = float(TEX_HEIGHT) / float(lineHeight+1);
        float texPos = (drawStart - H_2 + lineHeight / 2) * step;

        //DRAW FLOOR
        float floorXWall, floorYWall; //x, y position of the floor texel at the bottom of the wall
        if(side == 0 && rayDirX > 0) {
            floorXWall = mapX;
            floorYWall = mapY + wallX;
        } else if(side == 0 && rayDirX < 0) {
            floorXWall = mapX + 1.0;
            floorYWall = mapY + wallX;
        } else if(side == 1 && rayDirY > 0) {
            floorXWall = mapX + wallX;
            floorYWall = mapY;
        } else {
            floorXWall = mapX + wallX;
            floorYWall = mapY + 1.0;
        }

        //Brightness stuff
        float aoAlpha = getAlpha(drawEnd, perpDistance, floorXWall, floorYWall);

        //DRAW WALLS
        for(int y=drawStart; y<=drawEnd; ++y) {
            int texY = int(texPos) & (TEX_HEIGHT-1); //Mask to not overflow
            texPos += step;
            uint16_t c = textures[gameMap[mapY][mapX]][texY*TEX_HEIGHT + texX];

            // Side-based darkening flips from left to right as the camera changes
            // orientation, which creates the visible grey strip that follows the wall
            // hit direction instead of the actual scene brightness.
            const float wallShade = SDL_clamp(1.0f - perpDistance * 0.05f, 0.35f, 1.0f);
            c = smoothDarken(c, wallShade);

            if(x == W_2) {
                player.blockCenterX = mapX;
                player.blockCenterY = mapY;
            }

            if (FOG_ACTIVE) {
                auto yAlpha = float(texY)/(TEX_HEIGHT-1);
                float alpha = 1.0 + (aoAlpha - 1.0) * yAlpha;
                c = smoothDarken(c, SDL_clamp(alpha, 0.0, 1.0));
                Screen::drawPixel(x, y, blend(c, fogColor, blendDist));
            } else {
                Screen::drawPixel(x, y, c);
            }
        }

        Zbuffer[x] = perpDistance;

        //DRAW FLOOR
        if(drawEnd < 0) 
            drawEnd = H; 
        for(int y=drawEnd+1; y<H; ++y) {
            float currentDist = H / (2.0 * y - H);
            float weight = currentDist / perpDistance;

            float currentFloorX = weight * floorXWall + (1.0 - weight) * pos.x;
            float currentFloorY = weight * floorYWall + (1.0 - weight) * pos.y;

            int tx = int(float(FLOOR_WIDTH) * currentFloorX) & (FLOOR_WIDTH-1);
            int ty = int(float(FLOOR_HEIGHT) * currentFloorY) & (FLOOR_HEIGHT-1);

            uint16_t c = floorTexture[FLOOR_WIDTH*ty + tx];

            int floorX = int(currentFloorX), floorY = int(currentFloorY);
            int aoVertex[4] = {
                !!(gameMap[floorY-1][floorX-1]) + !!(gameMap[floorY-1][floorX]) + !!(gameMap[floorY][floorX-1]), //Top Left
                !!(gameMap[floorY-1][floorX]) + !!(gameMap[floorY-1][floorX+1]) + !!(gameMap[floorY][floorX+1]), //Top Right
                !!(gameMap[floorY][floorX-1]) + !!(gameMap[floorY+1][floorX-1]) + !!(gameMap[floorY+1][floorX]), //Bottom Left
                !!(gameMap[floorY][floorX+1]) + !!(gameMap[floorY+1][floorX+1]) + !!(gameMap[floorY+1][floorX]), //Bottom Right
            };

            float aoColors[4];
            for(int i=0; i<4; ++i)
                aoColors[i] = AO_Levels[aoVertex[i]];

            float xPer = float(tx)/(FLOOR_WIDTH-1), yPer = float(ty)/(FLOOR_HEIGHT-1);
            auto alpha = interpolate(aoColors, xPer, yPer);
            if (FOG_ACTIVE) {
                c = smoothDarken(c, SDL_clamp(alpha, 0.0, 1.0));
                aoAlpha = __min(aoAlpha, alpha);

                float floorFogDist = 255/fmaxf(currentDist, 1.0);
                Screen::drawPixel(x, y, blend(c, fogColor, floorFogDist));
            } else {
                Screen::drawPixel(x, y, c);
            }
        }

        //DRAW SKY
        for(int y=0; y<drawStart; ++y) {
            Screen::drawPixel(x, y, fogColor);
        }
    }
}