/* save.c - JSON (cJSON) save/load. */
#include "save.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../third_party/cjson/cJSON.h"

char *fr_save_serialize(const FrWorld *w, const FrPlayerState *p)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "version", FR_SAVE_VERSION);
    cJSON_AddNumberToObject(root, "seed_hi", (double)(uint32_t)(w->seed >> 32));
    cJSON_AddNumberToObject(root, "seed_lo", (double)(uint32_t)(w->seed & 0xffffffffu));
    cJSON_AddNumberToObject(root, "tick", w->tick);
    cJSON_AddNumberToObject(root, "rng_state_hi", (double)(uint32_t)(w->rng.state >> 32));
    cJSON_AddNumberToObject(root, "rng_state_lo", (double)(uint32_t)(w->rng.state & 0xffffffffu));
    cJSON_AddNumberToObject(root, "rng_inc_hi", (double)(uint32_t)(w->rng.inc >> 32));
    cJSON_AddNumberToObject(root, "rng_inc_lo", (double)(uint32_t)(w->rng.inc & 0xffffffffu));
    cJSON *ship = cJSON_AddObjectToObject(root, "ship");
    cJSON_AddNumberToObject(ship, "money", w->ship.money);
    cJSON_AddNumberToObject(ship, "loan", w->ship.loan);
    cJSON_AddNumberToObject(ship, "fuel", w->ship.fuel);
    cJSON_AddNumberToObject(ship, "wear", w->ship.wear);
    cJSON_AddNumberToObject(ship, "engine_level", w->ship.engine_level);
    cJSON *hold = cJSON_AddArrayToObject(ship, "hold");
    for (int i = 0; i < FR_MAX_COMMODITIES; i++)
        cJSON_AddItemToArray(hold, cJSON_CreateNumber(w->ship.hold[i]));
    cJSON_AddNumberToObject(root, "port", w->port);
    cJSON_AddNumberToObject(root, "travel_active", w->travel.active);
    cJSON_AddNumberToObject(root, "travel_from", w->travel.from_port);
    cJSON_AddNumberToObject(root, "travel_to", w->travel.to_port);
    cJSON_AddNumberToObject(root, "travel_total", w->travel.ticks_total);
    cJSON_AddNumberToObject(root, "travel_done", w->travel.ticks_done);
    cJSON *stock = cJSON_AddArrayToObject(root, "market_stock");
    for (int p = 0; p < FR_MAX_PORTS; p++) {
        cJSON *row = cJSON_CreateArray();
        cJSON_AddItemToArray(stock, row);
        for (int c = 0; c < FR_MAX_COMMODITIES; c++)
            cJSON_AddItemToArray(row, cJSON_CreateNumber(w->market_stock[p][c]));
    }
    cJSON *contracts = cJSON_AddArrayToObject(root, "contracts");
    for (int i = 0; i < w->contract_count; i++) {
        const FrContract *c = &w->contracts[i];
        cJSON *jc = cJSON_CreateObject();
        cJSON_AddNumberToObject(jc, "id", c->id);
        cJSON_AddNumberToObject(jc, "dest", c->dest_port);
        cJSON_AddNumberToObject(jc, "commodity", c->commodity);
        cJSON_AddNumberToObject(jc, "units", c->units);
        cJSON_AddNumberToObject(jc, "reward", c->reward);
        cJSON_AddNumberToObject(jc, "deadline", c->deadline_day);
        cJSON_AddNumberToObject(jc, "state", c->state);
        cJSON_AddItemToArray(contracts, jc);
    }
    cJSON *pl = cJSON_AddObjectToObject(root, "player");
    cJSON_AddNumberToObject(pl, "deck", p->deck);
    cJSON_AddNumberToObject(pl, "on_station", p->on_station);
    cJSON_AddNumberToObject(pl, "docked", p->docked);
    cJSON_AddNumberToObject(pl, "px", p->px);
    cJSON_AddNumberToObject(pl, "py", p->py);
    cJSON_AddNumberToObject(pl, "angle", p->angle);
    cJSON_AddNumberToObject(pl, "ui_state", p->ui_state);

    char *s = cJSON_Print(root);
    cJSON_Delete(root);
    return s;
}

int fr_save_parse(const char *json, FrWorld *w, FrPlayerState *p)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) return -1;
    int rc = -2;
    do {
        const cJSON *v = cJSON_GetObjectItem(root, "version");
        if (!cJSON_IsNumber(v) || (int)v->valuedouble != FR_SAVE_VERSION) break;
        v = cJSON_GetObjectItem(root, "seed_hi");
        if (!cJSON_IsNumber(v)) break;
        memset(w, 0, sizeof(*w));
        w->seed = ((uint64_t)(uint32_t)v->valuedouble << 32) |
                  (uint64_t)(uint32_t)cJSON_GetObjectItem(root, "seed_lo")->valuedouble;
        w->tick = (int)cJSON_GetObjectItem(root, "tick")->valuedouble;
        w->rng.state = ((uint64_t)(uint32_t)cJSON_GetObjectItem(root, "rng_state_hi")->valuedouble << 32) |
                       (uint64_t)(uint32_t)cJSON_GetObjectItem(root, "rng_state_lo")->valuedouble;
        w->rng.inc = ((uint64_t)(uint32_t)cJSON_GetObjectItem(root, "rng_inc_hi")->valuedouble << 32) |
                     (uint64_t)(uint32_t)cJSON_GetObjectItem(root, "rng_inc_lo")->valuedouble;
        const cJSON *ship = cJSON_GetObjectItem(root, "ship");
        w->ship.money = (int)cJSON_GetObjectItem(ship, "money")->valuedouble;
        w->ship.loan = (int)cJSON_GetObjectItem(ship, "loan")->valuedouble;
        w->ship.fuel = (int)cJSON_GetObjectItem(ship, "fuel")->valuedouble;
        w->ship.wear = (int)cJSON_GetObjectItem(ship, "wear")->valuedouble;
        w->ship.engine_level = (int)cJSON_GetObjectItem(ship, "engine_level")->valuedouble;
        const cJSON *holdarr = cJSON_GetObjectItem(ship, "hold");
        const cJSON *item = NULL;
        int i = 0;
        cJSON_ArrayForEach(item, holdarr) {
            if (i >= FR_MAX_COMMODITIES) break;
            w->ship.hold[i++] = (int)item->valuedouble;
        }
        w->port = (int)cJSON_GetObjectItem(root, "port")->valuedouble;
        w->travel.active = (int)cJSON_GetObjectItem(root, "travel_active")->valuedouble;
        w->travel.from_port = (int)cJSON_GetObjectItem(root, "travel_from")->valuedouble;
        w->travel.to_port = (int)cJSON_GetObjectItem(root, "travel_to")->valuedouble;
        w->travel.ticks_total = (int)cJSON_GetObjectItem(root, "travel_total")->valuedouble;
        w->travel.ticks_done = (int)cJSON_GetObjectItem(root, "travel_done")->valuedouble;
        const cJSON *stock = cJSON_GetObjectItem(root, "market_stock");
        const cJSON *row = stock ? stock->child : NULL;
        for (int pr = 0; pr < FR_MAX_PORTS && row; pr++, row = row->next) {
            const cJSON *cell = row->child;
            for (int cr = 0; cr < FR_MAX_COMMODITIES && cell; cr++, cell = cell->next)
                w->market_stock[pr][cr] = (int)cell->valuedouble;
        }
        const cJSON *contracts = cJSON_GetObjectItem(root, "contracts");
        const cJSON *jc = NULL;
        w->contract_count = 0;
        cJSON_ArrayForEach(jc, contracts) {
            if (w->contract_count >= FR_MAX_CONTRACTS) break;
            FrContract *c = &w->contracts[w->contract_count++];
            c->id = (int)cJSON_GetObjectItem(jc, "id")->valuedouble;
            c->dest_port = (int)cJSON_GetObjectItem(jc, "dest")->valuedouble;
            c->commodity = (int)cJSON_GetObjectItem(jc, "commodity")->valuedouble;
            c->units = (int)cJSON_GetObjectItem(jc, "units")->valuedouble;
            c->reward = (int)cJSON_GetObjectItem(jc, "reward")->valuedouble;
            c->deadline_day = (int)cJSON_GetObjectItem(jc, "deadline")->valuedouble;
            c->state = (int)cJSON_GetObjectItem(jc, "state")->valuedouble;
        }
        const cJSON *pl = cJSON_GetObjectItem(root, "player");
        p->deck = (int)cJSON_GetObjectItem(pl, "deck")->valuedouble;
        p->on_station = (int)cJSON_GetObjectItem(pl, "on_station")->valuedouble;
        p->docked = (int)cJSON_GetObjectItem(pl, "docked")->valuedouble;
        p->px = (float)cJSON_GetObjectItem(pl, "px")->valuedouble;
        p->py = (float)cJSON_GetObjectItem(pl, "py")->valuedouble;
        p->angle = (float)cJSON_GetObjectItem(pl, "angle")->valuedouble;
        p->ui_state = (int)cJSON_GetObjectItem(pl, "ui_state")->valuedouble;
        rc = 0;
    } while (0);
    cJSON_Delete(root);
    return rc;
}

int fr_save_write_file(const char *path, const FrWorld *w, const FrPlayerState *p)
{
    char *s = fr_save_serialize(w, p);
    if (!s) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) {
        free(s);
        return -2;
    }
    size_t len = strlen(s);
    size_t written = fwrite(s, 1, len, f);
    fclose(f);
    free(s);
    return written == len ? 0 : -3;
}

int fr_save_read_file(const char *path, FrWorld *w, FrPlayerState *p)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > 4 * 1024 * 1024) {
        fclose(f);
        return -2;
    }
    char *buf = malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        return -3;
    }
    size_t rd = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[rd] = 0;
    int rc = fr_save_parse(buf, w, p);
    free(buf);
    return rc;
}
