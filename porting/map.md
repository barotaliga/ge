# Porting map

A WORKING DOCUMENT (`porting/` is deleted when porting ends). One row per engine file that was
ported: the reference module it reproduces and the reference identifiers it covers, so a search
for a reference name (`make find Q=generateLevel`) lands on the engine file. The engine's own
files never name their reference; this table is the only place the link is kept.

`make agents-check` holds this table: every engine file named exists, rows are sorted by path,
and no engine file appears twice. Add a row in the same PR that adds a ported file, and move
the row when the file moves.

| Engine file | Reference module | Reference identifiers |
| --- | --- | --- |
| `engine/core/heightfield.hpp` | `oss-game-framework/src/core/heightfield.ts` | createHeightfield fillField sampleField sampleFieldGradient fieldGradient fieldCell |
| `engine/core/math.hpp` | `oss-game-framework/src/core/math.ts` | angleDiff |
| `engine/core/noise.hpp` | `oss-game-framework/src/core/noise.ts` | valueNoise noiseField sampleNoise |
| `engine/core/num.hpp` | JavaScript number semantics | Math.round Math.imul Math.max Math.min Math.sign `\|0` `>>>0` `%` |
| `engine/core/num_text.cpp` | JavaScript `Number.prototype` | toFixed toString |
| `engine/core/num_text.hpp` | JavaScript `Number.prototype` | toFixed toString |
| `engine/core/output.cpp` | `oss-game-framework/src/core/output.ts` |  |
| `engine/core/output.hpp` | `oss-game-framework/src/core/output.ts` | recentLogs setOutputSink |
| `engine/core/prng.hpp` | `oss-game-framework/src/core/prng.ts` | pickIndex createRng |
| `engine/core/quat.hpp` | `oss-game-framework/src/core/quat.ts` | fromEuler toEuler fromAxisAngle |
| `engine/core/solar.cpp` | `oss-game-framework/src/core/solar.ts` |  |
| `engine/core/solar.hpp` | `oss-game-framework/src/core/solar.ts` | sunAt moonAt daylightWindow sunOnDate moonOnDate hourOfElevation |
| `engine/loop/run_clock.hpp` | `oss-game-framework/src/loop/run-clock.ts` | createRunClock MAX_FRAME_SECONDS |
| `plugins/mountain/analysis/cliffs.cpp` | `game5/engine/analysis/cliffs.ts` | checkCliffs checkDrops |
| `plugins/mountain/analysis/cliffs.hpp` | `game5/engine/analysis/cliffs.ts` | checkCliffs checkDrops |
| `plugins/mountain/analysis/crossings.cpp` | `game5/engine/analysis/crossings.ts` | selfCrossings |
| `plugins/mountain/analysis/crossings.hpp` | `game5/engine/analysis/crossings.ts` | selfCrossings |
| `plugins/mountain/analysis/index.cpp` | `game5/engine/analysis/index.ts` | analyzeLevel |
| `plugins/mountain/analysis/index.hpp` | `game5/engine/analysis/index.ts` | analyzeLevel |
| `plugins/mountain/mapgen/access_build.cpp` | `game5/engine/mapgen/access-build.ts` |  |
| `plugins/mountain/mapgen/access_build.hpp` | `game5/engine/mapgen/access-build.ts` |  |
| `plugins/mountain/mapgen/berm.hpp` | `game5/engine/mapgen/berm.ts` | bermProfile bermCrest |
| `plugins/mountain/mapgen/cliffs.cpp` | `game5/engine/mapgen/cliffs.ts` | layCliffs stampCliff |
| `plugins/mountain/mapgen/cliffs.hpp` | `game5/engine/mapgen/cliffs.ts` | cliffProfile cliffFootprint onCliff layCliffs stampCliff |
| `plugins/mountain/mapgen/digest.cpp` | `game5/engine/mapgen/digest.ts` | levelDigest |
| `plugins/mountain/mapgen/digest.hpp` | `game5/engine/mapgen/digest.ts` | levelDigest |
| `plugins/mountain/mapgen/drags.cpp` | `game5/engine/mapgen/drags.ts` |  |
| `plugins/mountain/mapgen/drags.hpp` | `game5/engine/mapgen/drags.ts` |  |
| `plugins/mountain/mapgen/drift.cpp` | `game5/engine/mapgen/drift.ts` | dealDrifts stampDrifts |
| `plugins/mountain/mapgen/drift.hpp` | `game5/engine/mapgen/drift.ts` | dealDrifts driftAt stampDrifts |
| `plugins/mountain/mapgen/drops.cpp` | `game5/engine/mapgen/drops.ts` | layDrops publishDrops |
| `plugins/mountain/mapgen/drops.hpp` | `game5/engine/mapgen/drops.ts` | layDrops publishDrops |
| `plugins/mountain/mapgen/forest.cpp` | `game5/engine/mapgen/forest.ts` | growForest |
| `plugins/mountain/mapgen/forest.hpp` | `game5/engine/mapgen/forest.ts` | growForest treeAge trunkRadius |
| `plugins/mountain/mapgen/generate.cpp` | `game5/engine/mapgen/generate.ts` | attemptLevel searchLevel compileLevel normalAt |
| `plugins/mountain/mapgen/generate.hpp` | `game5/engine/mapgen/generate.ts` | generateLevel subSeed compileLevel |
| `plugins/mountain/mapgen/grades.cpp` | `game5/engine/mapgen/grades.ts` | GRADES UNGRADED steepestSpan |
| `plugins/mountain/mapgen/grades.hpp` | `game5/engine/mapgen/grades.ts` | GRADES UNGRADED gradeRow pisteGradeOf steepestSpan verticalBand |
| `plugins/mountain/mapgen/kickers.cpp` | `game5/engine/mapgen/kickers.ts` | layTrackKickers layOffKickers stampKicker |
| `plugins/mountain/mapgen/kickers.hpp` | `game5/engine/mapgen/kickers.ts` | layTrackKickers publishTrackKickers layOffKickers stampKicker onKicker |
| `plugins/mountain/mapgen/lanes.cpp` | `game5/engine/mapgen/lanes.ts` |  |
| `plugins/mountain/mapgen/lanes.hpp` | `game5/engine/mapgen/lanes.ts` |  |
| `plugins/mountain/mapgen/lift_chain.cpp` | `game5/engine/mapgen/lift-chain.ts` | chainBottom chainWay wayTarget wayLevels rectOff pressWay pressBox |
| `plugins/mountain/mapgen/lift_chain.hpp` | `game5/engine/mapgen/lift-chain.ts` | chainBottom chainWay layChain wayFalls keepsChain chainOf offWay offQueue groomWay pressWay |
| `plugins/mountain/mapgen/lift_line.hpp` | `game5/engine/game/lift-line.ts` | ringFrame letGoOf queueLane LIFT_LOOK CORRAL_TAIL DRAG_ARM |
| `plugins/mountain/mapgen/links.cpp` | `game5/engine/mapgen/links.ts` |  |
| `plugins/mountain/mapgen/links.hpp` | `game5/engine/mapgen/links.ts` |  |
| `plugins/mountain/mapgen/massif.cpp` | `game5/engine/mapgen/massif.ts` | planMassif bakeMassif ridgeShare steepShare benchShare massifRow heightAt |
| `plugins/mountain/mapgen/massif.hpp` | `game5/engine/mapgen/massif.ts` | planMassif bakeMassif ridgeShare |
| `plugins/mountain/mapgen/net_index.cpp` | `game5/engine/mapgen/net-index.ts` |  |
| `plugins/mountain/mapgen/net_index.hpp` | `game5/engine/mapgen/net-index.ts` |  |
| `plugins/mountain/mapgen/network.cpp` | `game5/engine/mapgen/network.ts` |  |
| `plugins/mountain/mapgen/network.hpp` | `game5/engine/mapgen/network.ts` |  |
| `plugins/mountain/mapgen/network_build.cpp` | `game5/engine/mapgen/network-build.ts` |  |
| `plugins/mountain/mapgen/network_build.hpp` | `game5/engine/mapgen/network-build.ts` |  |
| `plugins/mountain/mapgen/network_stamp.cpp` | `game5/engine/mapgen/network-build.ts` | networkStamp onCore stampRun pastJunction |
| `plugins/mountain/mapgen/network_stamp.hpp` | `game5/engine/mapgen/network-build.ts` | networkStamp onCore stampRun NetworkStamp |
| `plugins/mountain/mapgen/query.cpp` | `game5/engine/mapgen/query.ts` | nearestTrackPoint nearestWithin |
| `plugins/mountain/mapgen/query.hpp` | `game5/engine/mapgen/query.ts` | nearestTrackPoint nearestWithin trackPointAt |
| `plugins/mountain/mapgen/regions.cpp` | `game5/engine/mapgen/regions.ts` | REGIONS treeKindAt scaleCount |
| `plugins/mountain/mapgen/regions.hpp` | `game5/engine/mapgen/regions.ts` | regionRow treeKindAt |
| `plugins/mountain/mapgen/resort.cpp` | `game5/engine/mapgen/resort.ts` | planResort placeStations SLOTS TILT TALL_TILT ROAD_ROW |
| `plugins/mountain/mapgen/resort.hpp` | `game5/engine/mapgen/resort.ts` | planResort placeStations ROAD_ROW LiftPlan |
| `plugins/mountain/mapgen/resort_build.cpp` | `game5/engine/mapgen/resort-build.ts` | attemptResort layChain pressPads |
| `plugins/mountain/mapgen/resort_build.hpp` | `game5/engine/mapgen/resort-build.ts` | attemptResort buildResort ResortAttempt |
| `plugins/mountain/mapgen/resort_graded.cpp` | `game5/engine/mapgen/resort-build.ts` | attemptResort scaledRow |
| `plugins/mountain/mapgen/resort_rules.hpp` | `game5/engine/mapgen/resort-rules.ts` | RESORT_RULES TREE_LINE_MASSIF |
| `plugins/mountain/mapgen/rules.hpp` | `game5/engine/mapgen/rules.ts` | LEVEL_RULES inBand withinBand bendFloor |
| `plugins/mountain/mapgen/run_start.cpp` | `game5/engine/mapgen/run-start.ts` |  |
| `plugins/mountain/mapgen/run_start.hpp` | `game5/engine/mapgen/run-start.ts` |  |
| `plugins/mountain/mapgen/spawn.cpp` | `game5/engine/mapgen/spawn.ts` | chooseStart gridOnTrack layCheckpoints |
| `plugins/mountain/mapgen/spawn.hpp` | `game5/engine/mapgen/spawn.ts` | chooseStart gridOnTrack layCheckpoints startGateArc |
| `plugins/mountain/mapgen/station_clear.cpp` | `game5/engine/mapgen/station-clear.ts` | bottomFootprint |
| `plugins/mountain/mapgen/station_clear.hpp` | `game5/engine/mapgen/station-clear.ts` | bottomFootprint |
| `plugins/mountain/mapgen/station_pad.cpp` | `game5/engine/mapgen/station-pad.ts` | pressPads levelPad cutApproach unloadRise relevelPads groomPads dragTop |
| `plugins/mountain/mapgen/station_pad.hpp` | `game5/engine/mapgen/station-pad.ts` | padShape pressPads relevelPads groomPads padSurface dragTop dragTopsOf unloadPoint cutApproach |
| `plugins/mountain/mapgen/summit_ramps.cpp` | `game5/engine/mapgen/summit-ramps.ts` |  |
| `plugins/mountain/mapgen/summit_ramps.hpp` | `game5/engine/mapgen/summit-ramps.ts` |  |
| `plugins/mountain/mapgen/sun.cpp` | `game5/engine/mapgen/sun.ts` | dealSun declinationOf |
| `plugins/mountain/mapgen/sun.hpp` | `game5/engine/mapgen/sun.ts` | declinationOf sunWindow dealSun |
| `plugins/mountain/mapgen/surface.cpp` | `game5/engine/mapgen/surface.ts` | layCrust foldSurface |
| `plugins/mountain/mapgen/surface.hpp` | `game5/engine/mapgen/surface.ts` | layCrust foldSurface |
| `plugins/mountain/mapgen/terrain.cpp` | `game5/engine/mapgen/terrain.ts` | planTerrain bakeCountry |
| `plugins/mountain/mapgen/terrain.hpp` | `game5/engine/mapgen/terrain.ts` | planTerrain bakeCountry descentAt flankAt |
| `plugins/mountain/mapgen/track.cpp` | `game5/engine/mapgen/track.ts` | drawPiste gradePiste stampCorridor bendRoom |
| `plugins/mountain/mapgen/track.hpp` | `game5/engine/mapgen/track.ts` | drawPiste gradePiste stampCorridor tightestBend windowGrades minSeparation |
| `plugins/mountain/mapgen/types.hpp` | `game5/engine/mapgen/types.ts` | Level TrackPoint Checkpoint Kicker Cliff TreeDef Weather Sun GenerateOptions |
| `plugins/mountain/mapgen/versions.hpp` | `game5/engine/mapgen/versions.ts` | generatorTraits GENERATOR_VERSIONS CURRENT_GENERATOR_VERSION singlePiste |
| `plugins/mountain/mapgen/weather.cpp` | `game5/engine/mapgen/weather.ts` | dealWeather windIn |
| `plugins/mountain/mapgen/weather.hpp` | `game5/engine/mapgen/weather.ts` | dealWeather sunsetOf WEATHER_KINDS |
| `plugins/racing/records.hpp` | `oss-game-framework/src/racing/records.ts` | splitGap noteRecord |
| `plugins/racing/standings.hpp` | `oss-game-framework/src/racing/standings.ts` | legProgress fieldOrder placeAmong |
| `plugins/racing/tape.cpp` | `oss-game-framework/src/racing/tape.ts` | encodeStream decodeStream |
| `plugins/racing/tape.hpp` | `oss-game-framework/src/racing/tape.ts` | snapAxis ControlTape |
| `tests/mountain_generate_test.cpp` |  | generateLevel levelDigest analyzeLevel |
