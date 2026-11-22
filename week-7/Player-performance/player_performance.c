#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Players_data.h"

#define TOTAL_TEAMS 10
#define NAME_LIMIT 50
#define TEAM_ROSTER_CAP 50
#define ID_UPPER_BOUND 1000

typedef enum {
    ROLE_NONE = 0,
    ROLE_BATSMAN = 1,
    ROLE_BOWLER = 2,
    ROLE_ALLROUNDER = 3
} PlayerRole;

typedef struct PlayerRecord {
    int id;
    char fullName[NAME_LIMIT + 1];
    int teamId;
    PlayerRole role;
    int runs;
    float batAvg;
    float strikeRate;
    int wickets;
    float economy;
    float perfScore;
    struct PlayerRecord *next;
} PlayerRecord;

typedef struct {
    int id;
    char title[NAME_LIMIT + 1];
    int rosterCount;
    float avgBatStrikeRate;
    double cumulativeBatSR;
    int activeBatters;
    PlayerRecord *roleLists[3];
} TeamSquad;

typedef struct {
    TeamSquad squads[TOTAL_TEAMS];
    int count;
} LeagueRegistry;

typedef struct {
    PlayerRecord *node;
    int squadIdx;
} MergeSlot;

static const char *ROLE_LABELS[] = { "Unknown", "Batsman", "Bowler", "All-rounder" };


static void flushInputBuffer() {
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF) {}
}

static int resolveTeamId(const char *teamTitle) {
    static const struct {
        const char *name;
        int id;
    } teamDirectory[TOTAL_TEAMS] = {
        {"India", 1}, {"Australia", 2}, {"England", 3}, {"Pakistan", 4},
        {"New Zealand", 5}, {"South Africa", 6}, {"Bangladesh", 7},
        {"Sri Lanka", 8}, {"Afghanistan", 9}, {"West Indies", 10}
    };

    for (int i = 0; i < TOTAL_TEAMS; i++) {
        if (strcmp(teamTitle, teamDirectory[i].name) == 0) {
            return teamDirectory[i].id;
        }
    }
    return -1;
}

static PlayerRole parseRoleLabel(const char *label) {
    if (strcmp(label, "Batsman") == 0) return ROLE_BATSMAN;
    if (strcmp(label, "Bowler") == 0) return ROLE_BOWLER;
    if (strcmp(label, "All-rounder") == 0) return ROLE_ALLROUNDER;
    return ROLE_NONE;
}

static float computePerformanceScore(PlayerRole role, float avg, float sr, int wkts, float econ) {
    float batComponent = (avg * sr) / 100.0f;
    switch (role) {
        case ROLE_BATSMAN:
            return batComponent;
        case ROLE_BOWLER:
            return (wkts * 2.0f) + (100.0f - econ);
        case ROLE_ALLROUNDER:
            return batComponent + (wkts * 2.0f);
        default:
            return 0.0f;
    }
}

static int locateSquadIndex(const LeagueRegistry *reg, int targetTeamId) {
    int left = 0;
    int right = reg->count - 1;

    while (left <= right) {
        int mid = left + ((right - left) >> 1);
        if (reg->squads[mid].id == targetTeamId) {
            return mid;
        } else if (reg->squads[mid].id < targetTeamId) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    return -1;
}

static int isDuplicatePlayerId(const LeagueRegistry *reg, int queryId) {
    for (int t = 0; t < reg->count; t++) {
        for (int r = 0; r < 3; r++) {
            const PlayerRecord *cursor = reg->squads[t].roleLists[r];
            while (cursor != NULL) {
                if (cursor->id == queryId) return 1;
                cursor = cursor->next;
            }
        }
    }
    return 0;
}


static PlayerRecord *allocatePlayer(int id, const char *name, int teamId, PlayerRole role,
                                    int runs, float avg, float sr, int wkts, float econ) {
    PlayerRecord *rec = (PlayerRecord *)malloc(sizeof(PlayerRecord));
    if (!rec) {
        fprintf(stderr, "Error: Unable to allocate memory for player record.\n");
        return NULL;
    }

    rec->id = id;
    strncpy(rec->fullName, name, NAME_LIMIT);
    rec->fullName[NAME_LIMIT] = '\0';
    rec->teamId = teamId;
    rec->role = role;
    rec->runs = runs;
    rec->batAvg = avg;
    rec->strikeRate = sr;
    rec->wickets = wkts;
    rec->economy = econ;
    rec->perfScore = computePerformanceScore(role, avg, sr, wkts, econ);
    rec->next = NULL;

    return rec;
}

static void attachPlayerToSquad(TeamSquad *squad, PlayerRecord *newRec) {
    int idx = (int)newRec->role - 1;
    PlayerRecord **tracer = &squad->roleLists[idx];

    while (*tracer != NULL && (*tracer)->perfScore >= newRec->perfScore) {
        tracer = &((*tracer)->next);
    }
    newRec->next = *tracer;
    *tracer = newRec;

    squad->rosterCount++;
    if (newRec->role == ROLE_BATSMAN || newRec->role == ROLE_ALLROUNDER) {
        squad->cumulativeBatSR += newRec->strikeRate;
        squad->activeBatters++;
        squad->avgBatStrikeRate = (float)(squad->cumulativeBatSR / squad->activeBatters);
    }
}

static void bootstrapLeagueData(LeagueRegistry *reg) {
    reg->count = TOTAL_TEAMS;

    for (int i = 0; i < TOTAL_TEAMS; i++) {
        reg->squads[i].id = i + 1;
        reg->squads[i].title[0] = '\0';
        reg->squads[i].rosterCount = 0;
        reg->squads[i].avgBatStrikeRate = 0.0f;
        reg->squads[i].cumulativeBatSR = 0.0;
        reg->squads[i].activeBatters = 0;
        for (int r = 0; r < 3; r++) {
            reg->squads[i].roleLists[r] = NULL;
        }
    }

    for (int i = 0; i < teamCount; i++) {
        int id = resolveTeamId(teams[i]);
        if (id >= 1 && id <= TOTAL_TEAMS) {
            strncpy(reg->squads[id - 1].title, teams[i], NAME_LIMIT);
            reg->squads[id - 1].title[NAME_LIMIT] = '\0';
        }
    }

    for (int i = 0; i < playerCount; i++) {
        int tId = resolveTeamId(players[i].team);
        PlayerRole role = parseRoleLabel(players[i].role);
        int sIdx = locateSquadIndex(reg, tId);

        if (sIdx == -1 || role == ROLE_NONE) {
            continue;
        }

        PlayerRecord *rec = allocatePlayer(
            players[i].id, players[i].name, tId, role,
            players[i].totalRuns, players[i].battingAverage,
            players[i].strikeRate, players[i].wickets, players[i].economyRate
        );

        if (rec != NULL) {
            attachPlayerToSquad(&reg->squads[sIdx], rec);
        }
    }
}

static void renderTableBanner(int showTeamCol) {
    printf("\n");
    if (showTeamCol) {
        printf("%-6s %-24s %-15s %-13s %-8s %-8s %-7s %-6s %-8s %-11s\n",
               "ID", "Name", "Team", "Role", "Runs", "Avg", "SR", "Wkts", "Econ", "Perf.Index");
        printf("-------------------------------------------------------------------------------------------------------------\n");
    } else {
        printf("%-6s %-24s %-13s %-8s %-8s %-7s %-6s %-8s %-11s\n",
               "ID", "Name", "Role", "Runs", "Avg", "SR", "Wkts", "Econ", "Perf.Index");
        printf("---------------------------------------------------------------------------------------------\n");
    }
}

static void renderPlayerLine(const LeagueRegistry *reg, const PlayerRecord *p, int showTeamCol) {
    if (showTeamCol) {
        int sIdx = locateSquadIndex(reg, p->teamId);
        const char *tName = (sIdx >= 0) ? reg->squads[sIdx].title : "N/A";
        printf("%-6d %-24.24s %-15.15s %-13s %-8d %-8.1f %-7.1f %-6d %-8.1f %-11.2f\n",
               p->id, p->fullName, tName, ROLE_LABELS[p->role],
               p->runs, p->batAvg, p->strikeRate, p->wickets, p->economy, p->perfScore);
    } else {
        printf("%-6d %-24.24s %-13s %-8d %-8.1f %-7.1f %-6d %-8.1f %-11.2f\n",
               p->id, p->fullName, ROLE_LABELS[p->role],
               p->runs, p->batAvg, p->strikeRate, p->wickets, p->economy, p->perfScore);
    }
}

static PlayerRole promptForRole() {
    int choice;
    printf("Select Role (1-Batsman, 2-Bowler, 3-All-rounder): ");
    if (scanf("%d", &choice) != 1 || choice < 1 || choice > 3) {
        flushInputBuffer();
        printf("Invalid role selection.\n");
        return ROLE_NONE;
    }
    return (PlayerRole)choice;
}


static void featureRegisterPlayer(LeagueRegistry *reg) {
    int teamId, playerId, runs, wkts;
    float avg, sr, econ;
    char playerName[NAME_LIMIT + 1];

    printf("Enter Team ID (1-%d): ", TOTAL_TEAMS);
    if (scanf("%d", &teamId) != 1) {
        flushInputBuffer();
        printf("Invalid Team ID input.\n");
        return;
    }

    int squadIdx = locateSquadIndex(reg, teamId);
    if (squadIdx == -1) {
        printf("Team ID %d does not exist.\n", teamId);
        return;
    }

    if (reg->squads[squadIdx].rosterCount >= TEAM_ROSTER_CAP) {
        printf("Squad %s is full (Max %d players).\n", reg->squads[squadIdx].title, TEAM_ROSTER_CAP);
        return;
    }

    printf("Enter Player ID (1-%d): ", ID_UPPER_BOUND);
    if (scanf("%d", &playerId) != 1 || playerId < 1 || playerId > ID_UPPER_BOUND) {
        flushInputBuffer();
        printf("Player ID must be between 1 and %d.\n", ID_UPPER_BOUND);
        return;
    }

    if (isDuplicatePlayerId(reg, playerId)) {
        printf("Player ID %d is already assigned.\n", playerId);
        return;
    }

    printf("Enter Player Name: ");
    if (scanf(" %50[^\n]", playerName) != 1) {
        flushInputBuffer();
        printf("Invalid name entered.\n");
        return;
    }

    PlayerRole role = promptForRole();
    if (role == ROLE_NONE) return;

    printf("Total Runs: ");
    if (scanf("%d", &runs) != 1 || runs < 0) {
        flushInputBuffer();
        printf("Invalid runs value.\n");
        return;
    }

    printf("Batting Average: ");
    if (scanf("%f", &avg) != 1 || avg < 0.0f) {
        flushInputBuffer();
        printf("Invalid batting average.\n");
        return;
    }

    printf("Strike Rate: ");
    if (scanf("%f", &sr) != 1 || sr < 0.0f) {
        flushInputBuffer();
        printf("Invalid strike rate.\n");
        return;
    }

    printf("Wickets: ");
    if (scanf("%d", &wkts) != 1 || wkts < 0) {
        flushInputBuffer();
        printf("Invalid wickets value.\n");
        return;
    }

    printf("Economy Rate: ");
    if (scanf("%f", &econ) != 1 || econ < 0.0f) {
        flushInputBuffer();
        printf("Invalid economy rate.\n");
        return;
    }

    PlayerRecord *newPlayer = allocatePlayer(playerId, playerName, teamId, role, runs, avg, sr, wkts, econ);
    if (!newPlayer) return;

    attachPlayerToSquad(&reg->squads[squadIdx], newPlayer);
    printf("Successfully registered %s to %s (Performance Index: %.2f).\n",
           newPlayer->fullName, reg->squads[squadIdx].title, newPlayer->perfScore);
}

static void featureShowSquadRoster(const LeagueRegistry *reg) {
    int teamId;
    printf("Enter Team ID (1-%d): ", TOTAL_TEAMS);
    if (scanf("%d", &teamId) != 1) {
        flushInputBuffer();
        printf("Invalid input.\n");
        return;
    }

    int squadIdx = locateSquadIndex(reg, teamId);
    if (squadIdx == -1) {
        printf("Team not found.\n");
        return;
    }

    const TeamSquad *squad = &reg->squads[squadIdx];
    printf("\nRoster for Team %s (ID: %d)\n", squad->title, squad->id);
    renderTableBanner(0);

    for (int r = 0; r < 3; r++) {
        const PlayerRecord *curr = squad->roleLists[r];
        while (curr != NULL) {
            renderPlayerLine(reg, curr, 0);
            curr = curr->next;
        }
    }

    printf("\nTotal Players: %d | Average Batting Strike Rate: %.2f\n",
           squad->rosterCount, squad->avgBatStrikeRate);
}

static void featureRankTeamsByStrikeRate(const LeagueRegistry *reg) {
    const TeamSquad *ranked[TOTAL_TEAMS];
    for (int i = 0; i < reg->count; i++) {
        ranked[i] = &reg->squads[i];
    }

    for (int i = 1; i < reg->count; i++) {
        const TeamSquad *key = ranked[i];
        int j = i - 1;
        while (j >= 0 && ranked[j]->avgBatStrikeRate < key->avgBatStrikeRate) {
            ranked[j + 1] = ranked[j];
            j--;
        }
        ranked[j + 1] = key;
    }

    printf("\nTeams Ranked by Average Batting Strike Rate\n");
    printf("%-8s %-22s %-15s %-15s\n", "Team ID", "Team Name", "Avg Bat SR", "Total Players");
    printf("---------------------------------------------------------------\n");
    for (int i = 0; i < reg->count; i++) {
        printf("%-8d %-22s %-15.2f %-15d\n",
               ranked[i]->id, ranked[i]->title,
               ranked[i]->avgBatStrikeRate, ranked[i]->rosterCount);
    }
}

static void featureShowTopKInSquad(const LeagueRegistry *reg) {
    int teamId, k;
    printf("Enter Team ID (1-%d): ", TOTAL_TEAMS);
    if (scanf("%d", &teamId) != 1) {
        flushInputBuffer();
        printf("Invalid input.\n");
        return;
    }

    int squadIdx = locateSquadIndex(reg, teamId);
    if (squadIdx == -1) {
        printf("Team not found.\n");
        return;
    }

    PlayerRole role = promptForRole();
    if (role == ROLE_NONE) return;

    printf("Enter K (number of top players): ");
    if (scanf("%d", &k) != 1 || k <= 0) {
        flushInputBuffer();
        printf("K must be greater than 0.\n");
        return;
    }

    const TeamSquad *squad = &reg->squads[squadIdx];
    const PlayerRecord *curr = squad->roleLists[(int)role - 1];

    printf("\nTop %d %s(s) in %s\n", k, ROLE_LABELS[role], squad->title);
    if (!curr) {
        printf("No players available in this category.\n");
        return;
    }

    renderTableBanner(0);
    int printed = 0;
    while (curr != NULL && printed < k) {
        renderPlayerLine(reg, curr, 0);
        curr = curr->next;
        printed++;
    }
}


static int isBetterSlot(const MergeSlot *a, const MergeSlot *b) {
    if (a->node->perfScore != b->node->perfScore) {
        return a->node->perfScore > b->node->perfScore;
    }
    return a->node->id < b->node->id;
}

static void siftDownHeap(MergeSlot heap[], int size, int rootIdx) {
    int best = rootIdx;
    int left = 2 * rootIdx + 1;
    int right = 2 * rootIdx + 2;

    if (left < size && isBetterSlot(&heap[left], &heap[best])) {
        best = left;
    }
    if (right < size && isBetterSlot(&heap[right], &heap[best])) {
        best = right;
    }
    if (best != rootIdx) {
        MergeSlot swapTmp = heap[rootIdx];
        heap[rootIdx] = heap[best];
        heap[best] = swapTmp;
        siftDownHeap(heap, size, best);
    }
}

static void featureMergeAllByRole(const LeagueRegistry *reg) {
    PlayerRole role = promptForRole();
    if (role == ROLE_NONE) return;

    MergeSlot heap[TOTAL_TEAMS];
    int heapLen = 0;
    int roleIdx = (int)role - 1;

    for (int i = 0; i < reg->count; i++) {
        if (reg->squads[i].roleLists[roleIdx] != NULL) {
            heap[heapLen].node = reg->squads[i].roleLists[roleIdx];
            heap[heapLen].squadIdx = i;
            heapLen++;
        }
    }

    if (heapLen == 0) {
        printf("No players found for role %s.\n", ROLE_LABELS[role]);
        return;
    }

    for (int i = (heapLen / 2) - 1; i >= 0; i--) {
        siftDownHeap(heap, heapLen, i);
    }

    printf("\nGlobal Ranking: %s(s) Across All Teams\n", ROLE_LABELS[role]);
    renderTableBanner(1);

    int totalShown = 0;
    while (heapLen > 0) {
        MergeSlot top = heap[0];
        renderPlayerLine(reg, top.node, 1);
        totalShown++;

        if (top.node->next != NULL) {
            heap[0].node = top.node->next;
        } else {
            heap[0] = heap[heapLen - 1];
            heapLen--;
        }

        if (heapLen > 0) {
            siftDownHeap(heap, heapLen, 0);
        }
    }
    printf("\nTotal %s(s) listed: %d\n", ROLE_LABELS[role], totalShown);
}

static void releaseLeagueMemory(LeagueRegistry *reg) {
    for (int t = 0; t < reg->count; t++) {
        for (int r = 0; r < 3; r++) {
            PlayerRecord *curr = reg->squads[t].roleLists[r];
            while (curr != NULL) {
                PlayerRecord *toFree = curr;
                curr = curr->next;
                free(toFree);
            }
            reg->squads[t].roleLists[r] = NULL;
        }
    }
}

int main() {
    LeagueRegistry registry;
    int option = 0;

    bootstrapLeagueData(&registry);
    printf("Initialized %d players across %d teams.\n", playerCount, teamCount);

    while (option != 6) {
        printf("\n============================================================\n");
        printf("            ICC ODI PLAYER PERFORMANCE ANALYZER\n");
        printf("============================================================\n");
        printf("1. Add Player to Team\n");
        printf("2. Display Players of a Specific Team\n");
        printf("3. Display Teams by Average Batting Strike Rate\n");
        printf("4. Display Top K Players of a Specific Team by Role\n");
        printf("5. Display All Players of a Specific Role Across All Teams\n");
        printf("6. Exit\n");
        printf("============================================================\n");
        printf("Choose an option: ");

        if (scanf("%d", &option) != 1) {
            flushInputBuffer();
            printf("Invalid input. Enter a number between 1 and 6.\n");
            continue;
        }

        switch (option) {
            case 1:
                featureRegisterPlayer(&registry);
                break;
            case 2:
                featureShowSquadRoster(&registry);
                break;
            case 3:
                featureRankTeamsByStrikeRate(&registry);
                break;
            case 4:
                featureShowTopKInSquad(&registry);
                break;
            case 5:
                featureMergeAllByRole(&registry);
                break;
            case 6:
                releaseLeagueMemory(&registry);
                printf("All allocated memory freed!\n");
                break;
            default:
                printf("Option out of range. Please choose 1-6.\n");
                break;
        }
    }

    return 0;
}