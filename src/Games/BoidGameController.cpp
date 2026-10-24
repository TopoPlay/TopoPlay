/* TopoPlay modification notice — 2026-10-23
 * Modified from upstream Magic-Sand.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
#include "../World/AnimalTerrain.h" // ANIMAL_LEVELS_V56
#include "../ModeButtonSettings.h" // MODE_BUTTONS_V37
/***********************************************************************
BoidGameController.cpp - Controller for a Sandbox animal game
Copyright (c) 2016-2017 Thomas Wolf and Rasmus R. Paulsen (people.compute.dtu.dk/rapa)

This file is part of the Magic Sand.

The Magic Sand is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The Magic Sand is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along
with the Augmented Reality Sandbox; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
***********************************************************************/


#include "BoidGameController.h"
#include "../World/LavaCover.h" // AUTO_VOLCANO_V1
#include <algorithm>

#include <string>
//#include <direct.h>

CBoidGameController::CBoidGameController()
{
	DataBaseDir = "boidGame/";
	setDebug(true);

	int xmin = 0;
	int ymin = 0;
	int xmax = 640;
	int ymax = 480;
		
	std::string SplashScreenFname = DataBaseDir + "art/BoidGameSplashScreen.png";
	if (!splashScreen.loadImage(SplashScreenFname))
	{
		ofLogVerbose("CBoidGameController()") << "could not read splash screen ";
	}
	GameDifficulty = 2;

	LastTimeEvent = ofGetElapsedTimef();
	SetupGameSequence();
	doFlippedDrawing = false;

    requestedFishCount = 0;
    requestedSharkCount = 0;
    requestedRabbitCount = 0;
}

CBoidGameController::~CBoidGameController()
{
}

void CBoidGameController::setup(std::shared_ptr<KinectProjector> const& k)
{
	kinectProjector = k;

    terrainHabitatMap.setup(kinectProjector);
    terrainHabitatMap.setSampleStep(4);
    terrainHabitatMap.setUpdateInterval(1.0f);

    spawnManager.setup(&terrainHabitatMap);
    vegetationManager.setup(
        &terrainHabitatMap,
        &spawnManager
    );

    // --------------------------------------------------------
    // Fish habitat profile
    //
    // The World system knows only the habitat requirements.
    // It does not know anything about the Fish class itself.
    // --------------------------------------------------------
    fishSpawnProfile.name = "fish";
    fishSpawnProfile.requireWater = true;
    fishSpawnProfile.maxElevation = -25.0f;
    fishSpawnProfile.requireLand = false;

    // Ignore tiny puddles.
    fishSpawnProfile.minRegionArea = 256.0f;

    // Approximate habitat required per fish.
    // Nearly the entire calibrated sandbox being water
    // supports slightly more than the GUI maximum of 200.
    fishSpawnProfile.areaPerObject = 160.0f;

    // --------------------------------------------------------
    // Shark habitat profile
    //
    // Sharks require substantially larger connected water
    // habitat than fish.
    // --------------------------------------------------------
    sharkSpawnProfile.name = "shark";
    sharkSpawnProfile.requireWater = true;
    sharkSpawnProfile.maxElevation = -25.0f;
    sharkSpawnProfile.requireLand = false;

    // Small ponds may support fish but not sharks.
    sharkSpawnProfile.minRegionArea = 2500.0f;

    // Approximate habitat requirement per shark.
    sharkSpawnProfile.areaPerObject = 3500.0f;

    // --------------------------------------------------------
    // Rabbit habitat profile
    //
    // Rabbits require connected dry land. Tiny islands are
    // ignored, and the actual population scales automatically
    // with the amount of usable land.
    // --------------------------------------------------------
    rabbitSpawnProfile.name = "rabbit";
    rabbitSpawnProfile.requireWater = false;
    rabbitSpawnProfile.requireLand = true;

    // Ignore very small isolated patches of land.
    rabbitSpawnProfile.minRegionArea = 800.0f;

    // Approximate usable land required per rabbit.
    rabbitSpawnProfile.areaPerObject = 1400.0f;

	ofTrueTypeFont::setGlobalDpi(72);
	if (!scoreFont.loadFont("verdana.ttf", 64))
		std::cout << "Could not read font verdana.ttf" << std::endl;
	if (!scoreFontSmall.loadFont("verdana.ttf", 32))
		std::cout << "Could not read font verdana.ttf" << std::endl;
	if (!nameFont.loadFont("verdana.ttf", 128))
		std::cout << "Could not read font verdana.ttf" << std::endl;

	// Vehicles
	showMotherFish = false;
	showMotherRabbit = false;
	motherPlatformSize = 30;
	doFlippedDrawing = kinectProjector->getProjectionFlipped();

	setupGui();
}



void CBoidGameController::updateBOIDS()
{
	// Set static varible that indicate if all BOIDS should be drawn flipped
	Vehicle::setDrawFlipped(doFlippedDrawing);

	if (kinectProjector->isImageStabilized()) {

        // ------------------------------------------------------------
        // WORLD/HABITAT DIAGNOSTIC ONLY
        //
        // Analyze terrain once per configured interval. This currently
        // does NOT alter spawning, population or animal behavior.
        // ------------------------------------------------------------
        terrainHabitatMap.updateIfNeeded();

        // Vegetation is a generic environment layer driven by
        // the cached terrain habitat map.
        vegetationManager.updateIfNeeded();

        // ----------------------------------------------------
        // Fish population reconciliation
        //
        // The slider is the requested maximum.
        // Habitat determines how many fish can actually exist.
        //
        // Run only once per second so changing terrain does not
        // cause population churn every application frame.
        // ----------------------------------------------------
        static float lastFishPopulationUpdate = -1000.0f;

        float fishPopulationNow = ofGetElapsedTimef();

        if (terrainHabitatMap.isReady() &&
            fishPopulationNow - lastFishPopulationUpdate >= 1.0f)
        {
            lastFishPopulationUpdate = fishPopulationNow;

            // -----------------------------------------------
            // Handle fish lifecycle requests through the
            // generic World spawning system.
            //
            // If there is no valid habitat, the request stays
            // pending and will be tried again next second.
            // -----------------------------------------------
            for (auto& f : fish)
            {
                if (!f.needsRespawn() && !f.needsDeepWaterRescue())
                    continue;

                ofVec2f respawnLocation;

                std::vector<ofVec2f> existingPositions;
                existingPositions.reserve(fish.size());

                for (const auto& otherFish : fish)
                {
                    const ofPoint& p =
                        otherFish.getLocation();

                    existingPositions.emplace_back(
                        p.x,
                        p.y
                    );
                }

                if (spawnManager.findBalancedSpawnLocation(
                        fishSpawnProfile,
                        existingPositions,
                        respawnLocation))
                {
                    f.respawnAt(respawnLocation);
                }
            }

            int targetFish =
                spawnManager.getTargetPopulation(
                    fishSpawnProfile,
                    requestedFishCount
                );

            while (static_cast<int>(fish.size()) > targetFish)
            {
                fish.pop_back();
            }

            while (static_cast<int>(fish.size()) < targetFish)
            {
                if (!addNewFish())
                    break;
            }

            std::cout
                << "[FISH HABITAT]"
                << " requested=" << requestedFishCount
                << " capacity="
                << spawnManager.getCapacity(fishSpawnProfile)
                << " actual=" << fish.size()
                << std::endl;
        }

        // ----------------------------------------------------
        // Rabbit ecological population reconciliation
        //
        // Terrain defines where rabbits can physically live.
        // Grass + shrubs define ecological carrying capacity.
        //
        // Existing rabbits are NOT removed merely for wandering
        // away from a plant. That avoids visible pop-in/pop-out.
        // New rabbits, however, must begin in vegetation habitat.
        // ----------------------------------------------------
        static float lastRabbitPopulationUpdate = -1000.0f;

        float rabbitPopulationNow =
            ofGetElapsedTimef();

        if (terrainHabitatMap.isReady() &&
            rabbitPopulationNow -
                lastRabbitPopulationUpdate >= 1.0f)
        {
            lastRabbitPopulationUpdate =
                rabbitPopulationNow;


            std::vector<EnvironmentFeatureType>
                rabbitVegetation =
            {
                EnvironmentFeatureType::Grass,
                EnvironmentFeatureType::Shrub
            };


            int terrainCapacity =
                spawnManager.getCapacity(
                    rabbitSpawnProfile
                );


            // Each rabbit requires roughly six grass/shrub
            // features somewhere in the available ecosystem.
            //
            // This provides an ecological carrying-capacity
            // ceiling independent of raw dry-land area.
            int vegetationCapacity =
                vegetationManager.getFeatureSupportCapacity(
                    rabbitVegetation,
                    6
                );


            int ecologicalCapacity =
                std::min(
                    terrainCapacity,
                    vegetationCapacity
                );


            int targetRabbits =
                std::min(
                    requestedRabbitCount,
                    ecologicalCapacity
                );


            // If habitat contracts, gradually reduce the stored
            // population to the new carrying capacity.
            while (
                static_cast<int>(rabbits.size()) >
                targetRabbits)
            {
                rabbits.pop_back();
            }


            // New rabbits are placed only in vegetation-supported
            // terrain via addNewRabbit().
            while (
                static_cast<int>(rabbits.size()) <
                targetRabbits)
            {
                if (!addNewRabbit())
                    break;
            }


            std::cout
                << "[RABBIT ECOLOGY]"
                << " requested="
                << requestedRabbitCount

                << " terrainCapacity="
                << terrainCapacity

                << " vegetationCapacity="
                << vegetationCapacity

                << " target="
                << targetRabbits

                << " actual="
                << rabbits.size()

                << std::endl;
        }


        // ----------------------------------------------------
        // Shark population/lifecycle reconciliation
        // ----------------------------------------------------
        static float lastSharkPopulationUpdate = -1000.0f;

        float sharkPopulationNow = ofGetElapsedTimef();

        if (terrainHabitatMap.isReady() &&
            sharkPopulationNow - lastSharkPopulationUpdate >= 1.0f)
        {
            lastSharkPopulationUpdate = sharkPopulationNow;

            for (auto& shark : sharks)
            {
                if (!shark.needsRespawn() && !shark.needsDeepWaterRescue())
                    continue;

                ofVec2f respawnLocation;

                std::vector<ofVec2f> existingPositions;
                existingPositions.reserve(sharks.size());

                for (const auto& otherShark : sharks)
                {
                    const ofPoint& p =
                        otherShark.getLocation();

                    existingPositions.emplace_back(
                        p.x,
                        p.y
                    );
                }

                if (spawnManager.findBalancedSpawnLocation(
                        sharkSpawnProfile,
                        existingPositions,
                        respawnLocation))
                {
                    shark.respawnAt(respawnLocation);
                }
            }

            int targetSharks =
                spawnManager.getTargetPopulation(
                    sharkSpawnProfile,
                    requestedSharkCount
                );

            while (static_cast<int>(sharks.size()) > targetSharks)
            {
                sharks.pop_back();
            }

            while (static_cast<int>(sharks.size()) < targetSharks)
            {
                if (!addNewShark())
                    break;
            }

            std::cout
                << "[SHARK HABITAT]"
                << " requested=" << requestedSharkCount
                << " capacity="
                << spawnManager.getCapacity(sharkSpawnProfile)
                << " actual=" << sharks.size()
                << std::endl;
        }

        static float lastHabitatReport = -1000.0f;
        float habitatNow = ofGetElapsedTimef();

        if (terrainHabitatMap.isReady() &&
            habitatNow - lastHabitatReport >= 5.0f)
        {
            lastHabitatReport = habitatNow;

            const auto& regions = terrainHabitatMap.getRegions();

            int waterRegions = 0;
            int landRegions = 0;

            float totalWaterArea = 0.0f;
            float totalLandArea = 0.0f;

            std::cout
                << "[HABITAT] regions=" << regions.size();

            for (const auto& region : regions)
            {
                if (region.water)
                {
                    waterRegions++;
                    totalWaterArea += region.area;
                }
                else
                {
                    landRegions++;
                    totalLandArea += region.area;
                }
            }

            std::cout
                << " waterRegions=" << waterRegions
                << " waterArea=" << totalWaterArea
                << " landRegions=" << landRegions
                << " landArea=" << totalLandArea
                << std::endl;

            for (const auto& region : regions)
            {
                if (!region.water)
                    continue;

                std::cout
                    << "  [WATER REGION]"
                    << " id=" << region.id
                    << " area=" << region.area
                    << " cells=" << region.cellCount
                    << " elevAvg=" << region.averageElevation
                    << " slopeAvg=" << region.averageSlope
                    << std::endl;
            }
        }

        uint64_t logicStart = ofGetElapsedTimeMicros();

		for (auto & f : fish) {
			f.applyBehaviours(showMotherFish, fish, dangerBOIDS);
			f.update();
		}

        // Vegetation that rabbits naturally prefer.
        static const std::vector<EnvironmentFeatureType>
            rabbitPreferredVegetation =
        {
            EnvironmentFeatureType::Grass,
            EnvironmentFeatureType::Shrub
        };


        for (auto & r : rabbits)
        {
            const ofPoint& rabbitLocation3D =
                r.getLocation();

            ofVec2f rabbitLocation(
                rabbitLocation3D.x,
                rabbitLocation3D.y
            );


            ofVec2f habitatTarget;
            float habitatDistance = 0.0f;


            // Inside about 28 Kinect pixels of grass/shrubs,
            // rabbits are content and wander normally.
            const float comfortableRadius =
                28.0f;

            // Beyond that, search for useful vegetation out to
            // approximately 110 Kinect pixels.
            const float searchRadius =
                110.0f;


            bool foundHabitat =
                vegetationManager.findNearestFeature(
                    rabbitPreferredVegetation,
                    rabbitLocation,
                    searchRadius,
                    habitatTarget,
                    habitatDistance
                );


            if (foundHabitat &&
                habitatDistance > comfortableRadius)
            {
                // Preference increases gradually with distance.
                //
                // 28 px  -> 0
                // 110 px -> 1
                float strength =
                    ofMap(
                        habitatDistance,
                        comfortableRadius,
                        searchRadius,
                        0.0f,
                        1.0f,
                        true
                    );


                r.setHabitatPreference(
                    true,
                    habitatTarget,
                    strength
                );
            }
            else
            {
                r.setHabitatPreference(
                    false,
                    rabbitLocation,
                    0.0f
                );
            }


            r.applyBehaviours(
                showMotherRabbit,
                rabbits
            );

            r.update();
        }

		dangerBOIDS.clear();

		for (auto & s : sharks) {
			s.applyBehaviours(fish);
			s.update();
			dangerBOIDS.push_back(
                DangerousBOID(
                    s.getLocation(),
                    s.getVelocity(),
                    s.getSize() * 4
                )
            );
		}

        updateStingrays();

        uint64_t logicEnd = ofGetElapsedTimeMicros();

        uint64_t drawStart = ofGetElapsedTimeMicros();
		drawVehicles();
        uint64_t drawEnd = ofGetElapsedTimeMicros();

        static uint64_t logicTotal = 0;
        static uint64_t drawTotal = 0;
        static uint64_t sampleCount = 0;
        static uint64_t lastReport = ofGetElapsedTimeMicros();

        logicTotal += logicEnd - logicStart;
        drawTotal += drawEnd - drawStart;
        sampleCount++;

        uint64_t now = ofGetElapsedTimeMicros();

        if (now - lastReport >= 2000000 && sampleCount > 0)
        {
            double n = static_cast<double>(sampleCount);

            std::cout
                << "[BOID-INNER] "
                << "Logic=" << (logicTotal / n) / 1000.0 << "ms "
                << "Draw=" << (drawTotal / n) / 1000.0 << "ms "
                << std::endl;

            logicTotal = 0;
            drawTotal = 0;
            sampleCount = 0;
            lastReport = now;
        }
	}
	else
	{
		static int unstableCount = 0;
		unstableCount++;

		if (unstableCount % 30 == 0)
		{
			std::cout << "BOIDS skipped - Kinect image not stabilized"
			          << std::endl;
		}
	}
}



void CBoidGameController::update()
{
	float resultTime = ofGetElapsedTimef();



	eGameState sequence = GameSequence[CurrentGameSequence];
	if (sequence == GAME_STATE_PLAYANDSHOWCOUNTDOWN)
	{
        uint64_t fboBeginStart = ofGetElapsedTimeMicros();
		fboVehicles.begin();
		ofClear(255, 255, 255, 0);
        uint64_t fboBeginEnd = ofGetElapsedTimeMicros();

		updateBOIDS();
		ComputeScores();
		DrawScoresOnFBO();
		PlayAndShowCountDown(resultTime);

        uint64_t fboEndStart = ofGetElapsedTimeMicros();
		fboVehicles.end();
        uint64_t fboEndEnd = ofGetElapsedTimeMicros();

        static uint64_t fboBeginTotal = 0;
        static uint64_t fboEndTotal = 0;
        static uint64_t fboSamples = 0;
        static uint64_t fboLastReport = ofGetElapsedTimeMicros();

        fboBeginTotal += fboBeginEnd - fboBeginStart;
        fboEndTotal += fboEndEnd - fboEndStart;
        fboSamples++;

        uint64_t fboNow = ofGetElapsedTimeMicros();

        if (fboNow - fboLastReport >= 2000000 && fboSamples > 0)
        {
            double n = static_cast<double>(fboSamples);

            std::cout
                << "[BOID-FBO] "
                << "BeginClear=" << (fboBeginTotal / n) / 1000.0 << "ms "
                << "End=" << (fboEndTotal / n) / 1000.0 << "ms "
                << std::endl;

            fboBeginTotal = 0;
            fboEndTotal = 0;
            fboSamples = 0;
            fboLastReport = fboNow;
        }
	}
	else if (sequence == GAME_STATE_SHOWINTERMIDEATERESULT)
	{
		// Do nothing in update
	}
	else if (sequence == GAME_STATE_SHOWFINALRESULT)
	{
		// Do nothing in update
	}
	else if (sequence == GAME_STATE_IDLE)
	{
        uint64_t fboBeginStart = ofGetElapsedTimeMicros();
		fboVehicles.begin();
		ofClear(255, 255, 255, 0);
        uint64_t fboBeginEnd = ofGetElapsedTimeMicros();

		updateBOIDS();

		fboVehicles.end();
	}

	int DeltaTime = GameSequenceTimings[CurrentGameSequence];
	if (DeltaTime > 0)  // -1 is infinite
	{
		if (resultTime - LastTimeEvent > DeltaTime)
		{
			CurrentGameSequence++;
			// Debug check
			if (CurrentGameSequence >= GameSequence.size())
				CurrentGameSequence = GameSequence.size() - 1;

			InitiateGameSequence();
		}
	}
    // MODE_BUTTONS_V37: GUI is updated once by ofApp, including fossil mode.
}

void CBoidGameController::ComputeScores()
{
	Player1Score = 0;
	Player2Score = 0;

	double xmid = kinectROI.x + kinectROI.width / 2;

	double FishMassP1 = 0;
	double FishMassP2 = 0;
	for (int i = 0; i < fish.size(); i++)
	{
		ofPoint loc = fish[i].getLocation();
		double size = fish[i].getSize();

		if (loc.x < xmid)
		{
			FishMassP2 += size;
		}
		else
		{
			FishMassP1 += size;
		}
	}
	Player1Food += FishMassP1 / 2000.0;
	Player2Food += FishMassP2 / 2000.0;

	double NrabP1 = 0;
	double NrabP2 = 0;

	for (int i = 0; i < rabbits.size(); i++)
	{
		ofPoint loc = rabbits[i].getLocation();

		if (loc.x < xmid)
		{
			NrabP2 += 1;
		}
		else
		{
			NrabP1 += 1;
		}
	}
	Player1Skins += NrabP1 / 200.0;
	Player2Skins += NrabP2 / 200.0;

	Player1Score = Player1Skins + Player1Food;
	Player2Score = Player2Skins + Player2Food;
}

void CBoidGameController::PlayAndShowCountDown(int resultTime)
{
	int DeltaTime = GameSequenceTimings[CurrentGameSequence];
	int timeleft = DeltaTime - (resultTime - LastTimeEvent);

	std::string timeString = ofToString(timeleft);

	double sW = scoreFont.stringWidth(timeString);
	double sH = scoreFont.stringHeight(timeString);

	double sx = projROI.x + (projROI.width / 2 - sW/2);
	double sy = projROI.y + 70;

	scoreFont.drawString(timeString, sx, sy);
}

void CBoidGameController::DrawScoresOnFBO()
{
	ofSetColor(250, 250, 210);


	std::string P1ScoreStr = 
		"Food: " + ofToString((int)Player1Food) + "\n" +
		"Skins: " + ofToString((int)Player1Skins) + "\n" +
		"Score: " + ofToString((int)(Player1Skins + Player1Food));

	double sW = scoreFontSmall.stringWidth(P1ScoreStr);
	double sH = scoreFontSmall.stringHeight(P1ScoreStr);

	double sx = projROI.x + (projROI.width / 4 - sW / 2);
	double sy = projROI.y + 70;

	scoreFontSmall.drawString(P1ScoreStr, sx, sy);

	std::string P2ScoreStr = 
		"Food: " + ofToString((int)Player2Food) + "\n" +
		"Skins: " + ofToString((int)Player2Skins) + "\n" +
		"Score: " + ofToString((int)(Player2Skins + Player2Food));



	sW = scoreFontSmall.stringWidth(P1ScoreStr);
	sH = scoreFontSmall.stringHeight(P1ScoreStr);

	sx = projROI.x + (3 * projROI.width / 4 - sW / 2);
	sy = projROI.y + 70;

	scoreFontSmall.drawString(P2ScoreStr, sx, sy);

	double xmid = kinectROI.x + kinectROI.width / 2;
	double ymid = kinectROI.y + kinectROI.height / 2;

	ofVec2f projMid = kinectProjector->kinectCoordToProjCoord(xmid, ymid);

	ofSetColor(0, 255, 255);
	ofSetLineWidth(3.0f);
	ofVec2f ps(projMid.x, projROI.getMinY());
	ofVec2f pe(projMid.x, projROI.getMaxY());

	ofDrawLine(ps, pe);
}


void CBoidGameController::DrawFinalScoresOnFBO()
{
	DrawScoresOnFBO();

	ofSetColor(255, 255, 255);
	Player1Score = Player1Skins + Player1Food;
	Player2Score = Player2Skins + Player2Food;

	std::string scorestr = "You won!";
	double sW = nameFont.stringWidth(scorestr);
	double sH = nameFont.stringHeight(scorestr);

	double sy = projROI.y + (projROI.height / 2 - sH / 2);
	double sx = 0;

	if (Player1Score > Player2Score)
	{
		sx = projROI.x + (projROI.width / 4 - sW / 2);
	}
	else
	{
		sx = projROI.x + (3 * projROI.width / 4 - sW / 2);

	}

	scoreFont.drawString(scorestr, sx, sy);
}

void CBoidGameController::drawMainWindow(float x, float y, float width, float height) 
{
    fboVehicles.draw(x, y, width, height);
    drawGuiOnly();
}

void CBoidGameController::drawGuiOnly()
{
    gui->draw();
}


void CBoidGameController::drawProjectorWindow()
{
	eGameState sequence = GameSequence[CurrentGameSequence];
	if (sequence == GAME_STATE_SHOWSPLASHSCREEN)
	{
		fboVehicles.draw(0, 0);
	}
	else if (sequence == GAME_STATE_PLAYANDSHOWCOUNTDOWN)
	{
		fboVehicles.draw(0, 0);
	}
	else if (sequence == GAME_STATE_SHOWINTERMIDEATERESULT)
	{
		fboVehicles.draw(0, 0);
	}
	else if (sequence == GAME_STATE_SHOWFINALRESULT)
	{
		fboVehicles.draw(0, 0);
	}
	else if (sequence == GAME_STATE_IDLE)
	{
		fboVehicles.draw(0, 0);
	}
}


bool  CBoidGameController::InitiateGameSequence()
{
	eGameState sequence = GameSequence[CurrentGameSequence];

	if (sequence == GAME_STATE_SHOWSPLASHSCREEN)
	{
		CreateSplashScreen();
		std::cout << "Created splash screen" << std::endl;
	}
	else if (sequence == GAME_STATE_PLAYANDSHOWCOUNTDOWN)
	{
		fboVehicles.begin();
		ofClear(255, 255, 255, 0);

		fboVehicles.end();
		fish.clear();
        requestedFishCount = 0;

		rabbits.clear();
        requestedRabbitCount = 0;
		sharks.clear(); stingrays.clear();
        requestedSharkCount = 0;
		showMotherFish = false;
		showMotherRabbit = false;

		int nFish = 50;
		int nRabbit = 10;
		int nShark = 2;

		if (GameDifficulty == 0)
		{
			nFish = 5;
			nRabbit = 1;
			nShark = 0;
		}
		else if (GameDifficulty == 1)
		{
			nFish = 15;
			nRabbit = 3;
			nShark = 1;
		}
		else if (GameDifficulty == 2)
		{
			nFish = 30;
			nRabbit = 6;
			nShark = 2;
		}
		else if (GameDifficulty == 3)
		{
			nFish = 50;
			nRabbit = 10;
			nShark = 2;
		}


		for (int i = 0; i < nFish; i++)
			addNewFish();

		for (int i = 0; i < nRabbit; i++)
			addNewRabbit();

		for (int i = 0; i < nShark; i++)
			addNewShark();

		Player1Food = 0;
		Player2Food = 0;
		Player1Skins = 0;
		Player2Skins = 0;

		UpdateGUI();
	}
	else if (sequence == GAME_STATE_SHOWINTERMIDEATERESULT)
	{
		//
	}
	else if (sequence == GAME_STATE_SHOWFINALRESULT)
	{
		fboVehicles.begin();
		ofClear(255, 255, 255, 0);

		DrawFinalScoresOnFBO();

		fboVehicles.end();
	}

	LastTimeEvent = ofGetElapsedTimef();
	return true;
}


bool CBoidGameController::CreateSplashScreen()
{
	double sW = splashScreen.getWidth();
	double sH = splashScreen.getHeight();
	if (sW == 0 || sH == 0)
	{
		ofLogVerbose("CreateSplashScreen") << "splash screen invalid";
		return false;
	}

	// pH = 800, pW = 1280,  sH = 600, sW = 800
	// scale = 800 / 600
	// Swn = 800 / 600 * 800
	double scale = projROI.getHeight() / sH;
	double sWn = scale * sW;
	double sHn = projROI.getHeight();

	if (sWn > projROI.getWidth())
	{
		scale = projROI.getWidth() / sW;
		sHn = scale * sH;
		sWn = projROI.getWidth();
	}

	double xs = projROI.x + (projROI.width / 2 - sWn / 2);
	double ys = projROI.y + (projROI.height / 2 - sHn / 2);

	fboVehicles.begin();
	ofClear(255, 255, 255, 0);
	ofBackground(255);

	splashScreen.draw(xs, ys, sWn, sHn);

	fboVehicles.end();
	return true;
}

bool CBoidGameController::StartGame(int difficulty)
{
	GameDifficulty = difficulty;
	if (GameSequence[CurrentGameSequence] != GAME_STATE_IDLE)
	{
		// Stop the game
		CurrentGameSequence = GameSequence.size() - 1; // Set to IDLE by default
		return false;
	}

	projROI = kinectProjector->getProjectorActiveROI();
	
	CurrentGameSequence = 0;
	InitiateGameSequence();

	return true;
}

bool CBoidGameController::StartSeekMotherGame()
{
	if (GameSequence[CurrentGameSequence] != GAME_STATE_IDLE)
	{
		// Stop the current game
		CurrentGameSequence = GameSequence.size() - 1; // Set to IDLE by default
	}
	fboVehicles.begin();
	ofClear(255, 255, 255, 0);

	fboVehicles.end();
	fish.clear();
	rabbits.clear();
	sharks.clear(); stingrays.clear();
	addMotherFish();
	addMotherRabbit();

	for (int i = 0; i < 20; i++)
		addNewFish();

	for (int i = 0; i < 10; i++)
		addNewRabbit();

	UpdateGUI();
	return true;
}

void CBoidGameController::setProjectorRes(ofVec2f& PR)
{
	projRes = PR;

	fboVehicles.allocate(projRes.x, projRes.y, GL_RGBA);
	fboVehicles.begin();
	ofClear(0, 0, 0, 255);
	fboVehicles.end();
}

void CBoidGameController::setKinectRes(ofVec2f& KR)
{
	kinectRes = KR;
}

void CBoidGameController::setKinectROI(ofRectangle &KROI)
{
	kinectROI = KROI;

    terrainHabitatMap.setROI(kinectROI);

	if(!boxDrawingPreference)doFlippedDrawing = kinectProjector->getProjectionFlipped(); // BOX_SETTINGS_V7
}

void CBoidGameController::setDebug(bool flag)
{
	debugOn = flag;

	if (flag)
	{
		std::string BaseDir = "data//BoidGame//DebugFiles//";

		// By default the application looks in data
		debugBaseDir = "BoidGame//DebugFiles//";
	}
}

void CBoidGameController::SetupGameSequence()
{
	GameSequence.push_back(GAME_STATE_SHOWSPLASHSCREEN);
	GameSequenceTimings.push_back(5);

	GameSequence.push_back(GAME_STATE_PLAYANDSHOWCOUNTDOWN);
	GameSequenceTimings.push_back(180);








	GameSequence.push_back(GAME_STATE_SHOWFINALRESULT);
	GameSequenceTimings.push_back(5);

	GameSequence.push_back(GAME_STATE_IDLE);
	GameSequenceTimings.push_back(-1); // Indefinite

	CurrentGameSequence = GameSequence.size()-1; // Set to IDLE by default
}

bool CBoidGameController::addNewFish()
{
    ofVec2f location;

    // Ask the generic World spawning system for a valid location.
    // Suitable disconnected water regions are selected according
    // to their usable habitat area.
    std::vector<ofVec2f> existingPositions;
    existingPositions.reserve(fish.size());

    for (const auto& existingFish : fish)
    {
        const ofPoint& p =
            existingFish.getLocation();

        existingPositions.emplace_back(
            p.x,
            p.y
        );
    }

    if (!spawnManager.findBalancedSpawnLocation(
            fishSpawnProfile,
            existingPositions,
            location))
    {
        return false;
    }

    auto f =
        Fish(
            kinectProjector,
            location,
            kinectROI,
            motherFish
        );

    f.setup();
    fish.push_back(f);

    return true;
}

bool CBoidGameController::addNewShark()
{
    ofVec2f location;

    std::vector<ofVec2f> existingPositions;
    existingPositions.reserve(sharks.size());

    for (const auto& existingShark : sharks)
    {
        const ofPoint& p =
            existingShark.getLocation();

        existingPositions.emplace_back(
            p.x,
            p.y
        );
    }

    if (!spawnManager.findBalancedSpawnLocation(
            sharkSpawnProfile,
            existingPositions,
            location))
    {
        return false;
    }

    auto s =
        Shark(
            kinectProjector,
            location,
            kinectROI,
            motherFish
        );

    s.setup();
    sharks.push_back(s);

    return true;
}


bool CBoidGameController::addNewRabbit()
{
    ofVec2f location;


    // Existing positions still feed SpawnManager's connected-region
    // balancing system.
    std::vector<ofVec2f> existingPositions;
    existingPositions.reserve(rabbits.size());

    for (const auto& existingRabbit : rabbits)
    {
        const ofPoint& p =
            existingRabbit.getLocation();

        existingPositions.emplace_back(
            p.x,
            p.y
        );
    }


    // Generic environmental features that currently define useful
    // rabbit cover/food habitat.
    std::vector<EnvironmentFeatureType> rabbitVegetation =
    {
        EnvironmentFeatureType::Grass,
        EnvironmentFeatureType::Shrub
    };


    // Require valid rabbit terrain AND nearby grass/shrub habitat.
    //
    // 22 Kinect pixels gives rabbits a useful vegetation zone
    // without requiring them to sit directly on top of a plant.
    if (!vegetationManager.findSpawnLocationNearFeatures(
            rabbitSpawnProfile,
            rabbitVegetation,
            22.0f,
            1,
            existingPositions,
            location))
    {
        return false;
    }


    auto r =
        Rabbit(
            kinectProjector,
            location,
            kinectROI,
            motherRabbit
        );

    r.setup();

    rabbits.push_back(r);

    return true;
}


bool CBoidGameController::addMotherFish() {
	int minborderDist = 40;
	ofRectangle internalBorders = kinectROI;
	internalBorders.scaleFromCenter((kinectROI.width - minborderDist) / kinectROI.width, (kinectROI.height - minborderDist) / kinectROI.height);

	// Try to set a location for the Fish mother outside of the water to be sure the fish cannot reach her without help
	ofVec2f location;
	if (!setRandomVehicleLocation(internalBorders, false, location)) {
		return false;
	}
	motherFish = location;

	// Set the mother Fish plateform location under the sea level
	motherFish.z = kinectProjector->elevationToKinectDepth(-10, motherFish.x, motherFish.y);
	for (auto & f : fish) {
		f.setMotherLocation(motherFish);
	}
	showMotherFish = true;
	return true;
}

bool CBoidGameController::addMotherRabbit() {
	int minborderDist = 40;
	ofRectangle internalBorders = kinectROI;
	internalBorders.scaleFromCenter((kinectROI.width - minborderDist) / kinectROI.width, (kinectROI.height - minborderDist) / kinectROI.height);

	// Set a location for the Rabbits mother inside of the water to be sure the rabbits cannot reach her without help
	ofVec2f location;
	if (!setRandomVehicleLocation(internalBorders, true, location)) {
		return false;
	}
	motherRabbit = location;

	// Set the mother Rabbit plateform location over the sea level
	motherRabbit.z = kinectProjector->elevationToKinectDepth(10, motherRabbit.x, motherRabbit.y);

	for (auto & r : rabbits) {
		r.setMotherLocation(motherRabbit);
	}
	showMotherRabbit = true;
	return true;
}

bool CBoidGameController::setRandomVehicleLocation(ofRectangle area, bool liveInWater, ofVec2f & location) {
	bool okwater = false;
	int count = 0;
	int maxCount = 100;
	while (!okwater && count < maxCount) {
		count++;
		float x = ofRandom(area.getLeft(), area.getRight());
		float y = ofRandom(area.getTop(), area.getBottom());
		bool insideWater = MagicSandAnimalTerrain::elevation(kinectProjector->elevationAtKinectCoord(x, y)) < 0;
		if ((insideWater && liveInWater) || (!insideWater && !liveInWater)) {
			location = ofVec2f(x, y);
			okwater = true;
		}
	}
	return okwater;
}

void CBoidGameController::drawVehicles()
{
    // Static environmental features are drawn before animals.
    vegetationManager.draw(kinectProjector);

    uint64_t t0, t1;

    static uint64_t motherTotal = 0;
    static uint64_t fishTotal = 0;
    static uint64_t rabbitTotal = 0;
    static uint64_t sharkTotal = 0;
    static uint64_t samples = 0;
    static uint64_t lastReport = ofGetElapsedTimeMicros();

    // Mother animals
    t0 = ofGetElapsedTimeMicros();

    if (showMotherFish)
        drawMotherFish();

    if (showMotherRabbit)
        drawMotherRabbit();

    t1 = ofGetElapsedTimeMicros();
    motherTotal += t1 - t0;


    // Fish
    t0 = ofGetElapsedTimeMicros();

    for (auto & f : fish) {
        if(!MagicSandLavaCover::map().touches(f.getLocation().x,f.getLocation().y,
            std::max(2.0f,float(f.getSize())*1.3f)))f.draw();
    }

    t1 = ofGetElapsedTimeMicros();
    fishTotal += t1 - t0;


    // Rabbits
    t0 = ofGetElapsedTimeMicros();

    for (auto & r : rabbits) {
        r.draw();
    }

    t1 = ofGetElapsedTimeMicros();
    rabbitTotal += t1 - t0;


    // Sharks
    t0 = ofGetElapsedTimeMicros();

    for (auto & s : sharks) {
        if(!MagicSandLavaCover::map().touches(s.getLocation().x,s.getLocation().y,
            std::max(2.0f,float(s.getSize())*1.3f)))s.draw();
    }

    t1 = ofGetElapsedTimeMicros();
    sharkTotal += t1 - t0;

    for(auto& ray:stingrays) if(!MagicSandLavaCover::map().touches(ray.getLocation().x,ray.getLocation().y,
        std::max(2.0f,float(ray.getSize())*1.3f)))ray.draw();


    samples++;

    uint64_t now = ofGetElapsedTimeMicros();

    if (now - lastReport >= 2000000 && samples > 0)
    {
        double n = static_cast<double>(samples);

        std::cout
            << "[ANIMAL-DRAW] "
            << "Fish=" << (fishTotal / n) / 1000.0 << "ms "
            << "Rabbits=" << (rabbitTotal / n) / 1000.0 << "ms "
            << "Sharks=" << (sharkTotal / n) / 1000.0 << "ms "
            << "Mothers=" << (motherTotal / n) / 1000.0 << "ms "
            << std::endl;

        fishTotal = 0;
        rabbitTotal = 0;
        sharkTotal = 0;
        motherTotal = 0;
        samples = 0;
        lastReport = now;
    }
}


void CBoidGameController::drawMotherFish()
{
	// Mother fish scale
	float sc = 10;
	float tailSize = 1 * sc;
	float fishLength = 2 * sc;
	float fishHead = tailSize;
	float tailangle = 0;

	ofPushMatrix();
	ofTranslate(kinectProjector->kinectCoordToProjCoord(motherFish.x + tailSize, motherFish.y));

	ofFill();
	ofSetColor(ofColor::blueSteel);
	ofDrawCircle(-0.5*sc, 0, motherPlatformSize);

	ofFill();
	ofSetColor(255);
	ofPolyline fish;
	fish.curveTo(ofPoint(-fishLength - tailSize*cos(tailangle + 0.8), tailSize*sin(tailangle + 0.8)));
	fish.curveTo(ofPoint(-fishLength - tailSize*cos(tailangle + 0.8), tailSize*sin(tailangle + 0.8)));
	fish.curveTo(ofPoint(-fishLength, 0));
	fish.curveTo(ofPoint(0, -fishHead));
	fish.curveTo(ofPoint(fishHead, 0));
	fish.curveTo(ofPoint(0, fishHead));
	fish.curveTo(ofPoint(-fishLength, 0));
	fish.curveTo(ofPoint(-fishLength - tailSize*cos(tailangle - 0.8), tailSize*sin(tailangle - 0.8)));
	fish.curveTo(ofPoint(-fishLength - tailSize*cos(tailangle - 0.8), tailSize*sin(tailangle - 0.8)));
	fish.close();
	ofSetLineWidth(2.0);
	fish.draw();
	ofSetColor(255);
	ofDrawCircle(0, 0, 5);
	ofPopMatrix();
}

void CBoidGameController::drawMotherRabbit()
{
	float sc = 2; // MotherRabbit scale
	ofPushMatrix();
	ofTranslate(kinectProjector->kinectCoordToProjCoord(motherRabbit.x + 5 * sc, motherRabbit.y));

	ofFill();
	ofSetColor(ofColor::green);
	ofDrawCircle(-5 * sc, 0, motherPlatformSize);

	ofFill();
	ofSetLineWidth(1.0);
	ofPath body;
	body.curveTo(ofPoint(-2 * sc, 5.5*sc));
	body.curveTo(ofPoint(-2 * sc, 5.5*sc));
	body.curveTo(ofPoint(-9 * sc, 7.5*sc));
	body.curveTo(ofPoint(-17 * sc, 0 * sc));
	body.curveTo(ofPoint(-9 * sc, -7.5*sc));
	body.curveTo(ofPoint(-2 * sc, -5.5*sc));
	body.curveTo(ofPoint(4 * sc, 0 * sc));
	body.curveTo(ofPoint(4 * sc, 0 * sc));
	body.close();
	body.setFillColor(0);
	body.draw();

	ofSetColor(255);
	ofDrawCircle(-19 * sc, 0, 2 * sc);

	ofPath head;
	head.curveTo(ofPoint(0, 1.5*sc));
	head.curveTo(ofPoint(0, 1.5*sc));
	head.curveTo(ofPoint(-3 * sc, 1.5*sc));
	head.curveTo(ofPoint(-9 * sc, 3.5*sc));
	head.curveTo(ofPoint(0, 5.5*sc));
	head.curveTo(ofPoint(8 * sc, 0));
	head.curveTo(ofPoint(0, -5.5*sc));
	head.curveTo(ofPoint(-9 * sc, -3.5*sc));
	head.curveTo(ofPoint(-3 * sc, -1.5*sc));
	head.curveTo(ofPoint(0, -1.5*sc));
	head.curveTo(ofPoint(0, -1.5*sc));
	head.close();
	head.setFillColor(255);
	head.draw();

	ofSetColor(0);
	ofDrawCircle(8.5*sc, 0, 1 * sc);

	ofPopMatrix();
	ofSetColor(255);
}


// ============================================================
// Projected sandbox population controls
// ============================================================

void CBoidGameController::setRequestedFishCount(int count)
{
    requestedFishCount =
        ofClamp(count, 0, 200);

    // OFF should feel immediate.
    if (requestedFishCount == 0)
    {
        fish.clear();
    }

    if (gui)
    {
        gui->getSlider("# of fish")->setValue(
            requestedFishCount
        );
    }

    std::cout
        << "[PROJECTED CONTROL] fish requested="
        << requestedFishCount
        << std::endl;
}


void CBoidGameController::setRequestedRabbitCount(int count)
{
    requestedRabbitCount =
        ofClamp(count, 0, 50);

    if (requestedRabbitCount == 0)
    {
        rabbits.clear();
    }

    if (gui)
    {
        gui->getSlider("# of rabbits")->setValue(
            requestedRabbitCount
        );
    }

    std::cout
        << "[PROJECTED CONTROL] rabbits requested="
        << requestedRabbitCount
        << std::endl;
}


void CBoidGameController::setRequestedSharkCount(int count)
{
    requestedSharkCount =
        ofClamp(count, 0, 10);

    if (requestedSharkCount == 0)
    {
        sharks.clear(); stingrays.clear();
    }

    if (gui)
    {
        gui->getSlider("# of sharks")->setValue(
            requestedSharkCount
        );
    }

    std::cout
        << "[PROJECTED CONTROL] sharks requested="
        << requestedSharkCount
        << std::endl;
}


int CBoidGameController::getRequestedFishCount() const
{
    return requestedFishCount;
}


int CBoidGameController::getRequestedRabbitCount() const
{
    return requestedRabbitCount;
}


int CBoidGameController::getRequestedSharkCount() const
{
    return requestedSharkCount;
}



void CBoidGameController::setupGui() {
	// instantiate and position the gui //
	gui = new ofxDatGui();



	gui->addSlider("# of fish", 0, 200, fish.size())->setPrecision(0);
	gui->addSlider("# of rabbits", 0, 50, rabbits.size())->setPrecision(0);
	gui->addSlider("# of sharks", 0, 10, sharks.size())->setPrecision(0);
	gui->addToggle("Fossil button displayed", MagicSandModeButtons::values().fossil);
	gui->addToggle("Dig button displayed", MagicSandModeButtons::values().dig);
	gui->addToggle("Draw flipped", doFlippedDrawing);
	gui->addButton("Remove all animals");

	gui->addHeader(":: Games ::", false);

	gui->onButtonEvent(this, &CBoidGameController::onButtonEvent);
	gui->onToggleEvent(this, &CBoidGameController::onToggleEvent);
	gui->onSliderEvent(this, &CBoidGameController::onSliderEvent);
	gui->setLabelAlignment(ofxDatGuiAlignment::CENTER);

	gui->setPosition(ofxDatGuiAnchor::BOTTOM_RIGHT); // You have to do it at the end
	gui->setAutoDraw(false); // troubles with multiple windows drawings on Windows

}

void CBoidGameController::UpdateGUI()
{
	gui->getSlider("# of fish")->setValue(requestedFishCount);
	gui->getSlider("# of rabbits")->setValue(requestedRabbitCount);
	gui->getSlider("# of sharks")->setValue(requestedSharkCount);
	gui->getToggle("Fossil button displayed")->setChecked(MagicSandModeButtons::values().fossil);
	gui->getToggle("Dig button displayed")->setChecked(MagicSandModeButtons::values().dig);
}

bool CBoidGameController::isIdle()
{
	return(GameSequence[CurrentGameSequence] == GAME_STATE_IDLE);
}

void CBoidGameController::onButtonEvent(ofxDatGuiButtonEvent e) {
	if (e.target->is("Remove all animals")) {
		fish.clear();
		rabbits.clear();
		sharks.clear(); stingrays.clear();
		showMotherFish = false;
		showMotherRabbit = false;
		gui->getSlider("# of fish")->setValue(0);
		gui->getSlider("# of rabbits")->setValue(0);


	}
}

void CBoidGameController::onToggleEvent(ofxDatGuiToggleEvent e) {
    if(e.target->is("Fossil button displayed"))MagicSandModeButtons::values().fossil=e.checked;
    else if(e.target->is("Dig button displayed"))MagicSandModeButtons::values().dig=e.checked;
    else if(e.target->is("Draw flipped"))doFlippedDrawing=e.checked;
}

void CBoidGameController::onSliderEvent(ofxDatGuiSliderEvent e) {
	if (e.target->is("# of fish")) {

        requestedFishCount =
            static_cast<int>(e.value + 0.5f);

        int targetFish =
            spawnManager.getTargetPopulation(
                fishSpawnProfile,
                requestedFishCount
            );

        while (static_cast<int>(fish.size()) > targetFish)
        {
            fish.pop_back();
        }

        while (static_cast<int>(fish.size()) < targetFish)
        {
            if (!addNewFish())
                break;
        }
	}
	else if (e.target->is("# of rabbits")) {

        requestedRabbitCount =
            static_cast<int>(e.value + 0.5f);

        int targetRabbits =
            spawnManager.getTargetPopulation(
                rabbitSpawnProfile,
                requestedRabbitCount
            );

        while (static_cast<int>(rabbits.size()) > targetRabbits)
        {
            rabbits.pop_back();
        }

        while (static_cast<int>(rabbits.size()) < targetRabbits)
        {
            if (!addNewRabbit())
                break;
        }
	}
	else if (e.target->is("# of sharks")) {

        requestedSharkCount =
            static_cast<int>(e.value + 0.5f);

        int targetSharks =
            spawnManager.getTargetPopulation(
                sharkSpawnProfile,
                requestedSharkCount
            );

        while (static_cast<int>(sharks.size()) > targetSharks)
        {
            sharks.pop_back();
        }

        while (static_cast<int>(sharks.size()) < targetSharks)
        {
            if (!addNewShark())
                break;
        }
	}
}

void CBoidGameController::updateStingrays()
{
    if(sharks.empty()) { stingrays.clear(); return; }
    if(!terrainHabitatMap.isReady()) return;
    const float now=ofGetElapsedTimef();
    if(now>=nextStingrayPopulationUpdate) {
        nextStingrayPopulationUpdate=now+.5f;
        while(stingrays.size()>sharks.size()) stingrays.pop_back();
        std::vector<ofVec2f> positions;
        for(const auto& r:stingrays)
            positions.emplace_back(r.getLocation().x,r.getLocation().y);
        for(auto& ray:stingrays) {
            if(!ray.needsDeepWaterRescue()) continue;
            ofVec2f where;
            if(spawnManager.findBalancedSpawnLocation(sharkSpawnProfile,positions,where)) {
                ray=Stingray(kinectProjector,where,kinectROI,motherFish);
                ray.setup();
            }
        }
        while(stingrays.size()<sharks.size()) {
            ofVec2f where;
            if(!spawnManager.findBalancedSpawnLocation(sharkSpawnProfile,positions,where)) break;
            Stingray ray(kinectProjector,where,kinectROI,motherFish);
            ray.setup();
            stingrays.push_back(ray);
            positions.push_back(where);
        }
    }
    for(auto& ray:stingrays) { ray.applyBehaviours(); ray.update(); }
}

void CBoidGameController::captureBoxGui(ofJson& j)const{
    j["animals_fish"]=requestedFishCount;j["animals_sharks"]=requestedSharkCount;j["animals_rabbits"]=requestedRabbitCount;
    j["animals_mother_fish"]=false;j["animals_mother_rabbit"]=false;j["animals_flipped"]=doFlippedDrawing;
    MagicSandModeButtons::capture(j); // MODE_BUTTONS_V37
}
void CBoidGameController::restoreBoxGui(const ofJson& j){
    setRequestedFishCount(ofClamp(j.value("animals_fish",200),0,200));setRequestedSharkCount(ofClamp(j.value("animals_sharks",10),0,10));setRequestedRabbitCount(ofClamp(j.value("animals_rabbits",0),0,50));
    boxDrawingPreference=true;doFlippedDrawing=j.value("animals_flipped",doFlippedDrawing);gui->getToggle("Draw flipped")->setChecked(doFlippedDrawing);
    showMotherFish=showMotherRabbit=false;boxMotherFishPending=boxMotherRabbitPending=false;boxMotherRetryAt=0;
    gui->getToggle("Fossil button displayed")->setChecked(MagicSandModeButtons::values().fossil);
    gui->getToggle("Dig button displayed")->setChecked(MagicSandModeButtons::values().dig);
}
void CBoidGameController::retryBoxMotherDefaults(){
    // MODE_BUTTONS_V37: retired mother-animal startup controls.
}

void CBoidGameController::updateOperatorGui(){gui->update();}
void CBoidGameController::refreshOperatorGui(){UpdateGUI();}
