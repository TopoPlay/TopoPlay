/* TopoPlay modification notice — 2026-10-23
 * Modified from upstream Magic-Sand.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
#include "../World/AnimalTerrain.h" // ANIMAL_LEVELS_V56
/***********************************************************************
vehicle.cpp - vehicle class (animals/BOIDS moving in the sandbox)
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

#include "vehicle.h"
#include <cmath>
#include <algorithm>

// Default value of static variable
bool Vehicle::DrawFlipped = false;

Vehicle::Vehicle(std::shared_ptr<KinectProjector> const& k, ofPoint slocation, ofRectangle sborders, bool sliveInWater, ofVec2f smotherLocation) {
    kinectProjector = k;
    liveInWater = sliveInWater;
    location = slocation;
    borders = sborders;
    globalVelocityChange.set(0, 0);
    velocity.set(0.0, 0.0);
    angle = 0;
    wandertheta = 0;
    mother = false;
    motherLocation = smotherLocation;
	spawnTime = GetTimeStamp();
	isFleeing = false;
}

void Vehicle::updateBeachDetection()
{
    if(liveInWater) { updateDeepWaterAvoidance(); return; }

    // ------------------------------------------------------------
    // Stable shoreline avoidance + recovery.
    //
    // Water animals:
    //     <= -3 mm = safe water
    //      > -3 mm = shoreline / unsafe shallow terrain
    //
    // Land animals:
    //     >= +3 mm = safe land
    //      < +3 mm = shoreline / unsafe wet terrain
    //
    // If an animal is already sitting in unsuitable terrain,
    // local gradient alone may not be enough to free it.
    //
    // In that case we search nearby terrain and steer toward the
    // best recovery direction:
    //
    //     water animals -> deeper / lower elevation
    //     land animals  -> higher / drier elevation
    // ------------------------------------------------------------

    const float shorelineMargin = 3.0f;

    ofPoint futureLocation = location;

    beachSlope = ofVec2f(0);
    beach = false;
    beachDist = 10;


    float currentElevation =
        MagicSandAnimalTerrain::elevation(kinectProjector->elevationAtKinectCoord(
            location.x,
            location.y
        ));


    bool currentlyUnsuitable;

    if (liveInWater)
    {
        currentlyUnsuitable =
            currentElevation > -shorelineMargin;
    }
    else
    {
        currentlyUnsuitable =
            currentElevation < shorelineMargin;
    }


    // ============================================================
    // RECOVERY MODE
    //
    // The animal is already in the transition/sand area.
    // Search around it instead of trusting a weak local gradient.
    // ============================================================
    if (currentlyUnsuitable)
    {
        beach = true;
        beachDist = 1;

        const int directionCount = 16;

        // Search more than one radius so an animal can escape a
        // broad, flat shallow patch.
        const float searchRadii[] =
        {
            6.0f,
            12.0f,
            20.0f,
            30.0f
        };

        ofVec2f bestDirection(0, 0);

        float bestElevation =
            currentElevation;

        bool foundBetterLocation = false;


        for (float radius : searchRadii)
        {
            for (int d = 0;
                 d < directionCount;
                 ++d)
            {
                float angle =
                    TWO_PI *
                    static_cast<float>(d) /
                    static_cast<float>(directionCount);

                ofVec2f candidate(
                    location.x +
                        cos(angle) * radius,

                    location.y +
                        sin(angle) * radius
                );


                if (!borders.inside(candidate))
                    continue;


                float elevation =
                    MagicSandAnimalTerrain::elevation(kinectProjector->
                        elevationAtKinectCoord(
                            candidate.x,
                            candidate.y
                        ));


                bool better;

                if (liveInWater)
                {
                    // Lower elevation = deeper water.
                    better =
                        elevation < bestElevation;
                }
                else
                {
                    // Higher elevation = safer dry land.
                    better =
                        elevation > bestElevation;
                }


                if (better)
                {
                    bestElevation =
                        elevation;

                    bestDirection =
                        candidate - location;

                    foundBetterLocation =
                        true;
                }
            }
        }


        if (foundBetterLocation &&
            bestDirection.lengthSquared() > 0.001f)
        {
            beachSlope =
                bestDirection;
        }
        else
        {
            // Fallback to the terrain gradient if there is no
            // clearly better nearby sample.
            beachSlope =
                kinectProjector->
                    gradientAtKinectCoord(
                        location.x,
                        location.y
                    );

            if (liveInWater)
                beachSlope *= -1;
        }

        return;
    }


    // ============================================================
    // NORMAL LOOK-AHEAD MODE
    //
    // Current position is okay. Predict the next several steps and
    // begin steering away before entering the shoreline.
    // ============================================================
    int i = 1;

    while (i < 10 && !beach)
    {
        float elevation =
            MagicSandAnimalTerrain::elevation(kinectProjector->
                elevationAtKinectCoord(
                    futureLocation.x,
                    futureLocation.y
                ));


        bool unsuitableTerrain;

        if (liveInWater)
        {
            unsuitableTerrain =
                elevation > -shorelineMargin;
        }
        else
        {
            unsuitableTerrain =
                elevation < shorelineMargin;
        }


        if (unsuitableTerrain)
        {
            beach = true;
            beachDist = i;

            beachSlope =
                kinectProjector->
                    gradientAtKinectCoord(
                        futureLocation.x,
                        futureLocation.y
                    );

            if (liveInWater)
                beachSlope *= -1;
        }


        futureLocation += velocity;
        i++;
    }
}

ofPoint Vehicle::bordersEffect(){
    ofPoint desired, futureLocation;
    
    // Predict location 10 (arbitrary choice) frames ahead
    futureLocation = location + velocity*10;
    
    ofPoint target = location;
    if (!internalBorders.inside(futureLocation)){ // Go to the opposite direction
        border = true;
        if (futureLocation.x < internalBorders.getLeft())
            target.x = borders.getRight();
        if (futureLocation.y < internalBorders.getTop())
            target.y = borders.getBottom();
        if (futureLocation.x > internalBorders.getRight())
            target.x = borders.getLeft();
        if (futureLocation.y > internalBorders.getBottom())
            target.y = borders.getTop();
    } else {
        border = false;
    }
    
    desired = target - location;
    desired.normalize();
    desired *= topSpeed;
    
    ofPoint velocityChange(0);
    velocityChange = desired - velocity;
    velocityChange.limit(maxVelocityChange);
    return velocityChange;
}

ofPoint Vehicle::wanderEffect(){
    
    ofPoint velocityChange, desired;
    
    wandertheta += ofRandom(-change,change);     // Randomly change wander theta
    
    ofPoint front = velocity;
    front.normalize();
    front *= wanderD;
    ofPoint circleloc = location + front;
    
	float h = front.angle(ofVec2f(1,0)); // We need to know the heading to offset wandertheta
    
    ofPoint circleOffSet = ofPoint(wanderR*cos(wandertheta+h),wanderR*sin(wandertheta+h));
    ofPoint target = circleloc + circleOffSet;
    
    desired = target - location;
    desired.normalize();
    desired *= topSpeed;
    
    velocityChange = desired - velocity;
    velocityChange.limit(maxVelocityChange);
    
    return velocityChange;
}

ofPoint Vehicle::slopesEffect(){
    ofPoint desired, velocityChange;
    
    desired = beachSlope;
    desired.normalize();
    desired *= topSpeed;
    if(beach){
        desired /= beachDist; // The closest the beach is, the more we want to avoid it
    }
    velocityChange = desired - velocity;
    velocityChange.limit(maxVelocityChange);
    
    return velocityChange;
}

ofPoint Vehicle::seekMotherEffect(){
    ofPoint desired;
    desired = motherLocation - location;
    
    float d = desired.length();
    desired.normalize();
    
    //If we are closer than XX pixels slow down
    if (d < 10) {
        desired *= ofMap(d,0,100,0,topSpeed);
        mother = true;
    } else {
        //Otherwise, proceed at maximum speed.
        desired *= topSpeed;
    }
    
    ofPoint velocityChange;
    velocityChange = desired - velocity;
    velocityChange.limit(maxVelocityChange);
    
    //If we are further than XX pixels we don't see the mother
    if (d > 100) {
        velocityChange = ofPoint(0);
    }
    
    return velocityChange;
}


ofPoint Vehicle::arrivalEffect(ofPoint target)
{
	ofPoint desired;
	desired = target - location;

	float d = desired.length();
	desired.normalize();

	//If we are closer than XX pixels slow down
	if (d < 10) {
		desired *= ofMap(d, 0, 100, 0, topSpeed);
	}
	else {
		//Otherwise, proceed at maximum speed.
		desired *= topSpeed;
	}

	ofPoint velocityChange;
	velocityChange = desired - velocity;
	velocityChange.limit(maxVelocityChange);

	return velocityChange;
}

std::vector<ofVec2f> Vehicle::getForces(void)
{
    std::vector<ofVec2f> Forces;
    Forces.push_back( separateF);
	Forces.push_back(alignF);
	Forces.push_back(cohesionF);
    Forces.push_back( seekF);
    Forces.push_back( bordersF);
    Forces.push_back( slopesF);
    Forces.push_back( wanderF);
    return Forces;
}

void Vehicle::setSizeAndSpeed(double sz)
{
	size = sz;
}

void Vehicle::applyVelocityChange(const ofPoint & velocityChange){
    globalVelocityChange += velocityChange;
}

int Vehicle::GetTimeStamp()
{
	time_t t = time(0);   // get time now
	struct tm * now = localtime(&t);
	return now->tm_sec + now->tm_min * 60 + now->tm_hour * 3600;
}

void Vehicle::update(){

    // FORWARD_SWIMMING_V1
    if (liveInWater)
    {
        const float dt=ofClamp(
            ofGetLastFrameTime(),1.0f/120.0f,1.0f/15.0f);

        const bool first=!swimHeadingReady;
        ofPoint wanted=velocity+globalVelocityChange;
        globalVelocityChange.set(0,0);
        wanted.limit(topSpeed);
        // Safety steering takes priority over hunting and wandering.
        if(beach && beachSlope.lengthSquared()>.0001f) {
            wanted.set(beachSlope.x,beachSlope.y);
            wanted.normalize();
            wanted*=topSpeed*.70f;
        }

        if (first) {
            if (velocity.lengthSquared()>0.0001f)
                angle=ofRadToDeg(atan2(velocity.y,velocity.x));
            else if (wanted.lengthSquared()>0.0001f)
                angle=ofRadToDeg(atan2(wanted.y,wanted.x));

            swimHeadingReady=true;
        }

        float speed=wanted.length();

        if (!mother || velocity.lengthSquared()>0.0001f) {
            if (speed>0.0001f && topSpeed>0) {
                float target=ofRadToDeg(atan2(wanted.y,wanted.x));
                float difference=
                    fmod(target-angle+540.0f,360.0f)-180.0f;

                // Keep a consistent turning side when reversing.
                if (fabs(difference)>150.0f)
                    difference=fabs(difference)*swimTurnSide;
                else if (fabs(difference)>10.0f)
                    swimTurnSide=difference>0 ? 1.0f : -1.0f;

                // Degrees per second: lower means wider turns.
                const float turnLimit=110.0f*dt;
                angle+=ofClamp(difference,-turnLimit,turnLimit);
                angle=fmod(angle+360.0f,360.0f);

                // Slow down during sharp turns, without reversing.
                float alignment=ofClamp(
                    cos(ofDegToRad(difference)),0.0f,1.0f);

                speed*=.35f+.65f*alignment;

                const float heading=ofDegToRad(angle);
                velocity.set(
                    cos(heading)*speed,
                    sin(heading)*speed);

                // Move only after the heading has been updated.
                const ofPoint next=location+velocity;
                if(canSwimAt(next)) {
                    location=next;
                } else if(!canSwimAt(location) && borders.inside(next.x,next.y)) {
                    // Terrain changed underneath this animal: allow escape
                    // across an equally shallow patch toward the cached refuge.
                    const float before=MagicSandAnimalTerrain::elevation(kinectProjector->elevationAtKinectCoord(location.x,location.y));
                    const float after=MagicSandAnimalTerrain::elevation(kinectProjector->elevationAtKinectCoord(next.x,next.y));
                    if(std::isfinite(before) && std::isfinite(after) && after<=before+1.0f)
                        location=next;
                    else velocity.set(0,0);
                } else {
                    // Keep turning in place until forward travel is safe.
                    velocity.set(0,0);
                }
            } else {
                velocity.set(0,0);
            }
        }

        const ofVec2f target=
            kinectProjector->kinectCoordToProjCoord(
                location.x,location.y);

        if (first) {
            projectorCoord=target;

            const float heading=ofDegToRad(angle);
            const ofVec2f ahead=
                kinectProjector->kinectCoordToProjCoord(
                    location.x+cos(heading)*3.0f,
                    location.y+sin(heading)*3.0f);

            const ofVec2f direction=ahead-target;
            if (direction.lengthSquared()>0.000001f)
                swimDrawAngle=ofRadToDeg(
                    atan2(direction.y,direction.x));
        } else {
            const ofVec2f previous=projectorCoord;

            projectorCoord+=(target-projectorCoord)*
                (1.0f-exp(-5.0f*dt));

            // Match the nose to actual visible movement.
            const ofVec2f travel=projectorCoord-previous;
            if (travel.lengthSquared()>0.000001f)
                swimDrawAngle=ofRadToDeg(
                    atan2(travel.y,travel.x));
        }

        return;
    }

	ofVec2f targetProjectorCoord =
    kinectProjector->kinectCoordToProjCoord(location.x, location.y);

if (projectorCoord.x == 0 && projectorCoord.y == 0)
{
    projectorCoord = targetProjectorCoord;
}
else
{
    projectorCoord += (targetProjectorCoord - projectorCoord) * 0.15f;
}
    if (!mother || velocity.lengthSquared() != 0)
    {
        velocity += globalVelocityChange;
        velocity.limit(topSpeed);
        location += velocity;
        globalVelocityChange *= 0;
        
        float desiredAngle = ofRadToDeg(atan2(velocity.y,velocity.x));
        float angleChange = desiredAngle - angle;
        angleChange += (angleChange > 180) ? -360 : (angleChange < -180) ? 360 : 0; // To take into account that the difference between -180 and 180 is 0 and not 360
        angleChange *= velocity.length();
        angleChange /= topSpeed;
        angleChange = std::max(std::min(angleChange, maxRotation), -maxRotation);
        angle += angleChange;
	float heading = ofDegToRad(angle);
float speed = velocity.length();

velocity.x = cos(heading) * speed;
velocity.y = sin(heading) * speed;
    }
}


//==============================================================
// Derived class Fish
//==============================================================

void Fish::setup(){
    minborderDist = 50;
    internalBorders = borders;
    internalBorders.scaleFromCenter((borders.width-minborderDist)/borders.width, (borders.height-minborderDist)/borders.height);
    
    wanderR = 10;         // Radius for our "wander circle"
    wanderD = 80;         // Distance for our "wander circle"
    change = 0.3;
    
    desiredseparation = 10;
    maxRotation = 13;
	isOldest = false;
    respawnRequested = false;
	maxAge = 120; // Max two minutes lifetime
	
	// Go back in time to create fish that are big at start
	spawnTime = GetTimeStamp() - ofRandom(maxAge);
	DeathAge = maxAge / 4 + ofRandom(3 * maxAge / 4);
	UpdateAgeAndSize();
}

void Fish::UpdateAgeAndSize()
{
	int currentAge = getCurrentAge();
	float minSize = 1;
	float maxSize = 6;
	double sz = ofMap(currentAge, 0, maxAge, minSize, maxSize, true);
	setSizeAndSpeed(sz);
    // FISH_GROWTH_CAP_V31: retain the original 1..6 range for every age.
}

int Fish::getCurrentAge()
{
	return GetTimeStamp() - spawnTime;
}

bool Fish::needsRespawn()
{
    return respawnRequested ||
           getCurrentAge() > DeathAge;
}


void Fish::requestRespawn()
{
    respawnRequested = true;
}


void Fish::respawnAt(const ofVec2f& newLocation)
{
    deepWaterTargetReady=false;
    deepWaterNextSearch=0;
    deepWaterAvoidUntil=0;
    deepWaterStrandedSince=-1;

    swimHeadingReady=false;

    // Habitat selection belongs to SpawnManager.
    // Fish only resets its own lifecycle and local state.
    location.set(newLocation.x, newLocation.y);

    spawnTime =
        GetTimeStamp() -
        ofRandom(maxAge / 10);

    DeathAge =
        maxAge / 4 +
        ofRandom(3 * maxAge / 4);

    respawnRequested = false;
    isOldest = false;
    isFleeing = false;
    beach = false;

    UpdateAgeAndSize();
}


ofPoint Fish::wanderEffect(){
    
    ofPoint velocityChange, desired;
    
    wandertheta += ofRandom(-change,change);     // Randomly change wander theta
    
    ofPoint front = velocity;
    front.normalize();
    front *= wanderD;
    ofPoint circleloc = location + front;
    
    float h = atan2(front.y, front.x); // Signed angle
    
    ofPoint circleOffSet = ofPoint(wanderR*cos(wandertheta+h),wanderR*sin(wandertheta+h));
    ofPoint target = circleloc + circleOffSet;
    
    desired = target - location;
    desired.normalize();
    desired *= topSpeed;
    
    velocityChange = desired - velocity;
    velocityChange.limit(maxVelocityChange);
    
    return velocityChange;
}


ofPoint Fish::fleeEffect(std::vector<DangerousBOID>& dangers)
{
	ofPoint SumVelocityChange;
	isFleeing = false;
	for (int i = 0; i < dangers.size(); i++)
	{
		ofPoint target = dangers[i].location;

		ofPoint desired;
		desired = location - target;

		float d = desired.length();

		if (d < dangers[i].size)
		{
			desired.normalize();

			desired *= topSpeed;

			ofPoint velocityChange = desired - velocity;

			SumVelocityChange += velocityChange;
			isFleeing = true;
		}
	}
	SumVelocityChange.limit(maxVelocityChange);
	return SumVelocityChange;
}


ofPoint Fish::separateEffect(vector<Fish>& vehicles)
{
	ofPoint velocityChange;
	int count = 0;
	ofPoint diff;
	vector<Fish>::iterator other;
	for (other = vehicles.begin(); other < vehicles.end(); other++)
	{
		desiredseparation = (size + other->size) * 1.5;

		float d = (location - other->getLocation()).length();
		if ((d > 0) && (d < desiredseparation)) {
			diff = location - other->getLocation();
			diff.normalize();
			diff /= d;
			velocityChange += diff;
			count++;
		}
	}
	if (count > 0) {
		velocityChange /= count;
		velocityChange.normalize();
		velocityChange *= topSpeed;

		velocityChange -= velocity;
		velocityChange.limit(maxVelocityChange);
	}
	return velocityChange;
}

//// For every nearby boid in the system, calculate the average velocity
ofPoint Fish::alignEffect(std::vector<Fish>& vehicles)
{
	float neighbordist = 10 + size;
	ofPoint velocityChange;

	int count = 0;
	vector<Fish>::iterator other;
	for (other = vehicles.begin(); other < vehicles.end(); other++)
	{
		float d = (location - other->getLocation()).length();

		if ((d > 0) && (d < neighbordist)) 
		{
			velocityChange += other->velocity;
			count++;
		}
	}
	if (count > 0) {
		velocityChange /= count;
		velocityChange.normalize();
		velocityChange *= topSpeed;

		velocityChange -= velocity;
		velocityChange.limit(maxVelocityChange);
	}
	return velocityChange;
}


ofPoint Fish::cohesionEffect(std::vector<Fish>& vehicles)
{
	float neighbordist = 10 + size;
	ofPoint velocityChange;
	ofPoint desired;

	int count = 0;
	vector<Fish>::iterator other;
	for (other = vehicles.begin(); other < vehicles.end(); other++)
	{
		float d = (location - other->getLocation()).length();

		if ((d > 0) && (d < neighbordist))
		{
			desired += other->location;
			count++;
		}
	}
	if (count > 0) {
		desired /= count;
		velocityChange = desired - location;
		velocityChange.normalize();
		velocityChange *= topSpeed;

		velocityChange -= velocity;
		velocityChange.limit(maxVelocityChange);
	}
	return velocityChange;
}


void Fish::applyBehaviours(
    bool seekMother,
    std::vector<Fish>& vehicles,
    std::vector<DangerousBOID>& dangers)
{
    // Lifetime/respawn is handled externally by the controller.
    // Fish no longer chooses another fish's position itself.
    UpdateAgeAndSize();
    updateBeachDetection();


    // ============================================================
    // FAST SINGLE-PASS FLOCKING
    //
    // Original Magic-Sand scanned every fish THREE times:
    //
    //   separateEffect()
    //   alignEffect()
    //   cohesionEffect()
    //
    // With 200 fish that meant roughly:
    //
    //   200 * 200 * 3 = 120,000 neighbor checks/update
    //
    // We calculate all three effects during ONE neighbor scan.
    //
    // Squared distance comparisons also avoid sqrt() unless a
    // steering vector actually needs normalization later.
    // ============================================================

    ofVec2f separationSum(0, 0);
    ofVec2f alignmentSum(0, 0);
    ofVec2f cohesionPositionSum(0, 0);

    int separationCount = 0;
    int alignmentCount = 0;
    int cohesionCount = 0;

    const float neighborDistance = 10.0f + size;
    const float neighborDistanceSq =
        neighborDistance * neighborDistance;


    for (Fish& other : vehicles)
    {
        float dx = location.x - other.location.x;
        float dy = location.y - other.location.y;

        float distanceSq = dx * dx + dy * dy;

        // This is ourselves.
        if (distanceSq <= 0.000001f)
        {
            continue;
        }


        // --------------------------------------------------------
        // SEPARATION
        //
        // Preserve the original distance-weighted separation.
        //
        // Original:
        //
        //
        // which is mathematically:
        //
        // diff / d^2
        //
        // We already have d^2, so no sqrt() is required.
        // --------------------------------------------------------

        float separationDistance =
            (size + other.size) * 1.5f;

        float separationDistanceSq =
            separationDistance * separationDistance;

        if (distanceSq < separationDistanceSq)
        {
            float inverseDistanceSq =
                1.0f / distanceSq;

            separationSum.x +=
                dx * inverseDistanceSq;

            separationSum.y +=
                dy * inverseDistanceSq;

            ++separationCount;
        }


        // --------------------------------------------------------
        // ALIGNMENT + COHESION
        //
        // Both used exactly the same neighbor radius in the
        // original code, so calculate both in this same branch.
        // --------------------------------------------------------

        if (distanceSq < neighborDistanceSq)
        {
            alignmentSum += other.velocity;

            cohesionPositionSum +=
                other.location;

            ++alignmentCount;
            ++cohesionCount;
        }
    }


    // ============================================================
    // SEPARATION FORCE
    // ============================================================

    separateF.set(0, 0);

    if (separationCount > 0)
    {
        separateF =
            separationSum /
            static_cast<float>(separationCount);

        if (separateF.lengthSquared() > 0.000001f)
        {
            separateF.normalize();
            separateF *= topSpeed;

            separateF -= velocity;
            separateF.limit(maxVelocityChange);
        }
    }


    // ============================================================
    // ALIGNMENT FORCE
    // ============================================================

    alignF.set(0, 0);

    if (alignmentCount > 0)
    {
        alignF =
            alignmentSum /
            static_cast<float>(alignmentCount);

        if (alignF.lengthSquared() > 0.000001f)
        {
            alignF.normalize();
            alignF *= topSpeed;

            alignF -= velocity;
            alignF.limit(maxVelocityChange);
        }
    }

    // Original Magic-Sand strength.
    alignF *= 0.5f;


    // ============================================================
    // COHESION FORCE
    // ============================================================

    cohesionF.set(0, 0);

    if (cohesionCount > 0)
    {
        ofVec2f averagePosition =
            cohesionPositionSum /
            static_cast<float>(cohesionCount);

        cohesionF =
            averagePosition - location;

        if (cohesionF.lengthSquared() > 0.000001f)
        {
            cohesionF.normalize();
            cohesionF *= topSpeed;

            cohesionF -= velocity;
            cohesionF.limit(maxVelocityChange);
        }
    }

    // Original Magic-Sand strength.
    cohesionF *= 0.2f;


    // ============================================================
    // OTHER ORIGINAL BEHAVIOURS
    // ============================================================

    seekF.set(0, 0);

    if (seekMother)
    {
        seekF = seekMotherEffect();
    }

    bordersF = bordersEffect();
    slopesF = slopesEffect();
    wanderF = wanderEffect();


    if (!dangers.empty())
    {
        fleeF = fleeEffect(dangers);
    }
    else
    {
        fleeF.set(0, 0);
    }


    // Preserve original weighting.
    seekF *= 1.0f;
    bordersF *= 2.0f;
    slopesF *= 2.0f;
    wanderF *= 0.8f;


    // ============================================================
    // TERRAIN / LAND RESPONSE
    //
    // Keep the original behavior:
    //
    // Normal swimming remains forward-biased and curved.
    // Terrain steering only becomes important near beach/land.
    // ============================================================

    if (beach)
    {
        applyVelocityChange(slopesF);
    }

    if (border)
    {
        applyVelocityChange(bordersF);
    }


    // Schooling only nudges the existing direction.
    applyVelocityChange(separateF);
    applyVelocityChange(alignF);
    applyVelocityChange(cohesionF);

    applyVelocityChange(fleeF);


    // Preserve forward wandering unless seeking mother.
    if (seekF.lengthSquared() == 0)
    {
        applyVelocityChange(wanderF);
    }
    else
    {
        applyVelocityChange(seekF);
    }
}

void Fish::draw()
{
    ofPushMatrix();

    ofTranslate(projectorCoord);

    ofRotateDeg(swimDrawAngle);

    // Scale the entire fish once instead of recalculating every vertex.
    ofScale(size, size);

    float speedRatio = 0.0f;

    if (topSpeed > 0)
        speedRatio = ofClamp(
            velocity.length() / topSpeed,
            0.0f,
            1.0f
        );

    // Keep the animated swimming tail.
    float swimSpeed = 2.2f + speedRatio * 2.8f;
    float tailSwing =
        sin(ofGetElapsedTimef() * swimSpeed) * 0.70f;


    // --------------------------------------------------
    // COLORS
    // --------------------------------------------------

    ofColor bodyColor(70, 145, 185);
    ofColor bellyColor(145, 205, 220);
    ofColor finColor(45, 105, 145);

    if (mother)
    {
        float hue =
            fmod(ofGetElapsedTimef() * 45.0f, 255.0f);

        bodyColor.setHsb((int)hue, 190, 255);
        bellyColor.setHsb((int)hue, 100, 255);
        finColor.setHsb((int)hue, 220, 210);
    }

    if (DeathAge - getCurrentAge() < 10)
    {
        bodyColor = ofColor(110);
        bellyColor = ofColor(150);
        finColor = ofColor(80);
    }

    if (isOldest)
    {
        bodyColor = ofColor(220, 120, 145);
        bellyColor = ofColor(245, 185, 195);
        finColor = ofColor(175, 75, 100);
    }

    if (isFleeing)
    {
        bodyColor = ofColor(225, 75, 55);
        bellyColor = ofColor(255, 150, 100);
        finColor = ofColor(170, 40, 30);
    }


    ofFill();


    // --------------------------------------------------
    // SHADOW
    // --------------------------------------------------

    ofSetColor(0, 0, 0, 45);

    ofDrawEllipse(
        -0.30f,
         0.35f,
         3.6f,
         1.45f
    );


    // --------------------------------------------------
    // TAIL
    // --------------------------------------------------

    ofSetColor(finColor);

    float tx = -1.85f;

    ofDrawTriangle(
        tx, 0.0f,
        -3.05f, -0.90f + tailSwing,
        -2.75f, tailSwing
    );

    ofDrawTriangle(
        tx, 0.0f,
        -2.75f, tailSwing,
        -3.05f, 0.90f + tailSwing
    );


    // --------------------------------------------------
    // FINS
    // --------------------------------------------------

    ofDrawTriangle(
        -0.25f, -0.40f,
        -0.90f, -0.95f,
         0.35f, -0.50f
    );

    ofDrawTriangle(
        -0.25f, 0.40f,
        -0.90f, 0.95f,
         0.35f, 0.50f
    );


    // --------------------------------------------------
    // BODY
    //
    // Replaces the per-frame Bezier ofPath with one ellipse.
    // --------------------------------------------------

    ofSetColor(bodyColor);

    ofDrawEllipse(
        -0.15f,
         0.0f,
         3.45f,
         1.65f
    );


    // Slightly pointed nose.
    ofDrawTriangle(
        1.20f, -0.55f,
        1.75f,  0.00f,
        1.20f,  0.55f
    );


    // --------------------------------------------------
    // BELLY
    // --------------------------------------------------

    ofSetColor(bellyColor);

    ofDrawEllipse(
        0.10f,
        0.15f,
        2.20f,
        0.58f
    );


    // --------------------------------------------------
    // SIDE STRIPE
    // --------------------------------------------------

    ofSetColor(255, 255, 255, 90);
    ofSetLineWidth(1.0f);

    ofDrawLine(
        -1.35f, 0.0f,
         0.95f, 0.0f
    );


    // --------------------------------------------------
    // EYE
    // --------------------------------------------------

    ofSetColor(245);
    ofDrawCircle(1.05f, -0.28f, 0.17f);

    ofSetColor(10);
    ofDrawCircle(1.10f, -0.28f, 0.085f);

    ofSetColor(255);
    ofDrawCircle(1.13f, -0.31f, 0.030f);


    // --------------------------------------------------
    // MOUTH
    // --------------------------------------------------

    ofSetColor(35, 65, 75);
    ofSetLineWidth(1.0f);

    ofDrawLine(
        1.48f, -0.08f,
        1.62f,  0.0f
    );


    ofSetColor(255);
    ofFill();

    ofPopMatrix();
}


void Fish::setSizeAndSpeed(double sz)
{
	size = sz;
	topSpeed = size / 8;
	maxVelocityChange = size / 10;

	if (isFleeing)
	{
		topSpeed *= 1.5;
		maxVelocityChange *= 1.5;
	}
}

//==============================================================
// Derived class Rabbit
//==============================================================

void Rabbit::setup(){
    minborderDist = 50;
    internalBorders = borders;
    internalBorders.scaleFromCenter((borders.width-minborderDist)/borders.width, (borders.height-minborderDist)/borders.height);
    
    wanderR = 50;         // Radius for our "wander circle"
    wanderD = 0;         // Distance for our "wander circle"
    change = 1;
    
    desiredseparation = 24;
    maxVelocityChange = 1;
    maxRotation = 360;
    topSpeed = 1;
    velocityIncreaseStep = 2;
    maxStraightPath = 20;
    minVelocity = velocityIncreaseStep;
    
    minWaitingTime = 2;
    maxWaitingTime = 10;
    setWait = false;

    habitatPreferenceActive = false;
    habitatTarget.set(0, 0);
    habitatPreferenceStrength = 0.0f;
}


void Rabbit::setHabitatPreference(
    bool active,
    const ofVec2f& target,
    float strength)
{
    habitatPreferenceActive =
        active;

    habitatTarget =
        target;

    habitatPreferenceStrength =
        ofClamp(strength, 0.0f, 1.0f);
}


ofPoint Rabbit::wanderEffect(){
    
    ofPoint velocityChange, desired;
    
    wandertheta = ofRandom(-change,change);     // Randomly change wander theta
    
    float currDir = ofDegToRad(angle);
    ofPoint front = ofVec2f(cos(currDir), sin(currDir));
    
    front.normalize();
    front *= wanderD;
    ofPoint circleloc = location + front;
    
    
    ofPoint circleOffSet = ofPoint(wanderR*cos(wandertheta+currDir),wanderR*sin(wandertheta+currDir));
    ofPoint target = circleloc + circleOffSet;
    
    desired = target - location;
    desired.normalize();
    desired *= topSpeed;
    
    velocityChange = desired;// - velocity;
    velocityChange.limit(maxVelocityChange);
    
    return velocityChange;
}

void Rabbit::applyBehaviours(
    bool seekMother,
    std::vector<Rabbit>& vehicles)
{
    updateBeachDetection();

    seekF = ofVec2f(0);

    if (seekMother)
        seekF = seekMotherEffect();

    bordersF = bordersEffect();
    slopesF = slopesEffect();
    wanderF = wanderEffect();


    seekF *= 1;
    bordersF *= 0.5;
    slopesF *= 2;
    wanderF *= 1;


    // ------------------------------------------------------------
    // Gentle environmental-habitat preference.
    //
    // This never overrides shoreline, border, or mother behavior.
    // It simply biases normal wandering toward the target supplied
    // by the World/environment system.
    // ------------------------------------------------------------
    ofPoint habitatF(0, 0);

    const bool useHabitatPreference =
        habitatPreferenceActive &&
        !seekMother &&
        !mother;


    if (useHabitatPreference)
    {
        habitatF =
            habitatTarget - location;

        if (habitatF.lengthSquared() > 0.001f)
        {
            habitatF.normalize();

            // Keep this intentionally gentle.
            habitatF *=
                velocityIncreaseStep *
                habitatPreferenceStrength *
                0.65f;
        }
    }


    float currDir =
        ofDegToRad(angle);

    ofPoint oldDir =
        ofVec2f(
            cos(currDir),
            sin(currDir)
        );

    oldDir.scale(
        velocityIncreaseStep
    );


    if (beach)
    {
        oldDir.scale(
            velocityIncreaseStep /
            beachDist
        );
    }


    if (setWait)
    {
        waitCounter++;

        if (waitCounter > waitTime)
        {
            setWait = false;

            wanderF =
                wanderEffect();

            ofPoint newDir;


            if (seekF.lengthSquared() == 0)
            {
                newDir =
                    wanderF;
            }
            else
            {
                newDir =
                    seekF;
            }


            // Habitat preference applies only during ordinary
            // wandering. Shoreline/border safety has priority.
            if (!beach &&
                !border &&
                seekF.lengthSquared() == 0 &&
                useHabitatPreference)
            {
                newDir +=
                    habitatF;
            }


            if (border)
            {
                newDir +=
                    bordersF;
            }


            if (beach)
            {
                newDir +=
                    slopesF;
            }


            newDir.scale(
                velocityIncreaseStep
            );

            applyVelocityChange(
                newDir
            );


            currentStraightPathLength =
                0;

            angle =
                ofRadToDeg(
                    atan2(
                        newDir.y,
                        newDir.x
                    )
                );
        }
    }
    else
    {
        if (!beach &&
            !border &&
            !mother &&
            currentStraightPathLength <
                maxStraightPath)
        {
            ofPoint travelDir =
                oldDir;


            // While already moving, use only a fraction of the
            // preference force. This produces a gradual curve
            // rather than an abrupt turn.
            if (useHabitatPreference)
            {
                travelDir +=
                    habitatF * 0.35f;
            }


            applyVelocityChange(
                travelDir
            );

            currentStraightPathLength++;
        }
        else
        {
            // Decelerate before choosing a new direction.
            if (velocity.lengthSquared() >
                minVelocity * minVelocity)
            {
                applyVelocityChange(
                    -oldDir
                );
            }
            else
            {
                velocity =
                    ofPoint(0);

                setWait =
                    true;

                waitCounter =
                    0;

                waitTime =
                    ofRandom(
                        minWaitingTime,
                        maxWaitingTime
                    );


                // Do not make a rabbit sit around at a shoreline.
                if (beach)
                    waitTime = 0;


                // Far from preferred habitat: shorten the idle
                // period so it begins moving back sooner.
                if (useHabitatPreference &&
                    habitatPreferenceStrength > 0.65f)
                {
                    waitTime =
                        std::min(
                            waitTime,
                            2
                        );
                }
            }
        }
    }
}

void Rabbit::draw()
{
    ofPushMatrix();

    ofTranslate(projectorCoord);

    if (DrawFlipped)
        ofRotateDeg(180.0f + angle);
    else
        ofRotateDeg(angle);


    // --------------------------------------------------
    // COLORS
    // --------------------------------------------------

    ofColor bodyColor(235, 235, 225);
    ofColor detailColor(45, 45, 45);

    if (mother)
    {
        float hue =
            fmod(ofGetElapsedTimef() * 45.0f, 255.0f);

        bodyColor.setHsb((int)hue, 180, 255);

        detailColor.setHsb(
            ((int)hue + 128) % 255,
            210,
            210
        );
    }


    ofFill();


    // --------------------------------------------------
    // SHADOW
    // --------------------------------------------------

    ofSetColor(0, 0, 0, 40);

    ofDrawEllipse(
        -5.0f,
         2.0f,
         20.0f,
         8.0f
    );


    // --------------------------------------------------
    // BODY
    //
    // Replaces the original curved ofPath.
    // --------------------------------------------------

    ofSetColor(bodyColor);

    ofDrawEllipse(
        -7.0f,
         0.0f,
         20.0f,
         13.0f
    );


    // Rear haunch
    ofDrawCircle(
        -13.0f,
         1.0f,
         5.0f
    );


    // --------------------------------------------------
    // HEAD
    // --------------------------------------------------

    ofSetColor(detailColor);

    ofDrawEllipse(
        1.0f,
        0.0f,
        12.5f,
        9.0f
    );


    // --------------------------------------------------
    // EARS
    // --------------------------------------------------

    ofDrawTriangle(
        -1.0f, -3.0f,
         1.0f, -13.0f,
         3.0f, -3.0f
    );

    ofDrawTriangle(
         2.0f, -3.0f,
         6.0f, -11.5f,
         5.0f, -2.0f
    );


    // --------------------------------------------------
    // TAIL
    // --------------------------------------------------

    ofSetColor(bodyColor);

    ofDrawCircle(
        -18.0f,
         0.0f,
         2.4f
    );


    // --------------------------------------------------
    // NOSE
    // --------------------------------------------------

    ofSetColor(bodyColor);

    ofDrawCircle(
        7.2f,
        0.0f,
        1.2f
    );


    // --------------------------------------------------
    // EYE
    // --------------------------------------------------

    ofSetColor(255);

    ofDrawCircle(
        3.0f,
        -1.5f,
        1.0f
    );

    ofSetColor(10);

    ofDrawCircle(
        3.25f,
        -1.5f,
        0.45f
    );


    ofSetColor(255);
    ofFill();

    ofPopMatrix();
}


void Shark::setup()
{
	minborderDist = 50;
	internalBorders = borders;
	internalBorders.scaleFromCenter((borders.width - minborderDist) / borders.width, (borders.height - minborderDist) / borders.height);

	wanderR = 10;         // Radius for our "wander circle"
	wanderD = 80;         // Distance for our "wander circle"
	change = 0.3;

	desiredseparation = 10;
	maxRotation = 30;
	float minSize = 6;
	float maxSize = 10;
	size = ofRandom(minSize, maxSize);
	hunger = 0;
	isHunting = false;
	prayID = -1;
    respawnRequested = false;
	timeAtLastMeal = GetTimeStamp();
	setSizeAndSpeed(size);
}


void Shark::locatePray(std::vector<Fish>& vehicles)
{
	int huntRadius = size * 3;

	prayID = -1;
	int praySize = 3; // Minimum size to hunt 
	for (int i = 0; i < vehicles.size(); i++)
	{
		float d = (location - vehicles[i].getLocation()).length();
		if ((d > 0) && (d < huntRadius)) 
		{
			if (vehicles[i].getSize() > praySize)
			{
				praySize = vehicles[i].getSize();
				prayID = i;
			}
		}
	}
	if (prayID != -1)
	{
		isHunting = true;
		timeAtHuntStart = GetTimeStamp();
	}
}

//// Same as pursuit
// https://gamedevelopment.tutsplus.com/tutorials/understanding-steering-behaviors-pursuit-and-evade--gamedev-2946
ofPoint Shark::huntEffect(std::vector<Fish>& vehicles)
{
	if (prayID >= vehicles.size() || prayID == -1 || GetTimeStamp() - timeAtHuntStart > 10)
	{
		// Something happened to the target or you have been hunting for too long
		prayID = -1;
		isHunting = false;
		return ofPoint(0, 0);
	}

	ofPoint targetLoc = vehicles[prayID].getLocation();
	ofPoint targetVel = vehicles[prayID].getVelocity();
	float targetSize = vehicles[prayID].getSize();

	float d = (location - targetLoc).length();
	if (d < (targetSize+size) / 2)
	{
		// eat the fish!
		hunger -= targetSize;
		// currently just make the Pray fish very small - for testing
        vehicles[prayID].requestRespawn();

		if (hunger < 0)
		{
			hunger = 0;
			isHunting = 0;
			timeAtLastMeal = GetTimeStamp();
		}
		else
		{
			locatePray(vehicles);
		}

		return ofPoint(0, 0);
	}

	float T = d / topSpeed;
	ofPoint futureLoc = targetLoc + targetVel * T;

	return arrivalEffect(futureLoc);
}

void Shark::setSizeAndSpeed(double sz)
{
    size = sz;

    // Fast pursuit speed
    if (isHunting)
    {
        topSpeed = size / 4.0f;
        maxVelocityChange = size / 10.0f;
    }
    else
    {
        // Very slow normal cruising
        topSpeed = size / 20.0f;
        maxVelocityChange = size / 28.0f;

        // Occasional short burst of speed
        // About a 1% chance each update while cruising
        if (ofRandom(1.0f) < 0.01f)
        {
            topSpeed = size / 8.0f;
            maxVelocityChange = size / 16.0f;
        }
    }
}

// Very expensive N^2 - could be speeded up using a spatial search structure
ofPoint Shark::locateDensestFishPopulation(std::vector<Fish>& vehicles)
{
	double searchRad = 30;

	double maxDens = -1;
	int maxIdx = -1;
	for (int i = 0; i < vehicles.size(); i++)
	{
		double density = 0;
		for (int j = 0; j < vehicles.size(); j++)
		{
			if (i != j)
			{
				float d = (vehicles[i].getLocation() - vehicles[j].getLocation()).length();
				if (d < searchRad)
				{
					density += vehicles[j].getSize();
				}
			}
		}
		if (density > maxDens)
		{
			maxDens = density;
			maxIdx = i;
		}
	}
	if (maxIdx < 0)
	{
		// Something is wrong...
		return location;
	}

	return vehicles[maxIdx].getLocation();
}

bool Shark::needsRespawn() const
{
    return respawnRequested;
}


void Shark::requestRespawn()
{
    respawnRequested = true;
}


void Shark::respawnAt(const ofVec2f& newLocation)
{
    deepWaterTargetReady=false;
    deepWaterNextSearch=0;
    deepWaterAvoidUntil=0;
    deepWaterStrandedSince=-1;

    swimHeadingReady=false;

    // SpawnManager chooses the habitat.
    // Shark only resets its own lifecycle state.
    location.set(newLocation.x, newLocation.y);

    float minSize = 6;
    float maxSize = 10;

    hunger = 0;
    isHunting = false;
    prayID = -1;

    respawnRequested = false;
    beach = false;

    timeAtLastMeal = GetTimeStamp();

    size = ofRandom(minSize, maxSize);
    setSizeAndSpeed(size);
}


void Shark::applyBehaviours(std::vector<Fish>& vehicles)
{
	updateBeachDetection();

	setSizeAndSpeed(size);
	if (!isHunting)
	{
		int t = GetTimeStamp();
		hunger = (t - timeAtLastMeal) / 5;
		if (hunger > 5)
		{
			if (ofRandom(100) < 10)
			{
				locatePray(vehicles);
			}
		}
	}
	
	if (isHunting)
	{
		huntF = huntEffect(vehicles);
	}

	if (hunger > size * 3)
	{
        // Habitat/location selection is handled externally.
        requestRespawn();
	}


	seekF = ofVec2f(0);
	bordersF = bordersEffect();
	slopesF = slopesEffect();
	wanderF = wanderEffect();

	seekF *= 1;
	bordersF *= 2;
	slopesF *= 2;//2;
	wanderF *= 0.8;
	huntF *= 1.0;

	if (beach)
	{
		// Escaping shallow/sandy terrain takes priority over
		// hunting and wandering. Once back in safe water,
		// normal shark behavior resumes automatically.
		applyVelocityChange(slopesF);

		if (border)
			applyVelocityChange(bordersF);

		return;
	}

	if (border)
	{
		applyVelocityChange(bordersF);
	}


	applyVelocityChange(huntF);

	if (seekF.lengthSquared() == 0)
	{
		applyVelocityChange(wanderF);
	}
	else
	{
		applyVelocityChange(seekF);
	}
}

void Shark::draw()
{
    // TOPDOWN_SHARK_MESH_V1
    // Cached artwork; only positions and tint change while swimming.
    static ofMesh mesh;
    static std::vector<ofVec3f> rest;
    static std::vector<ofFloatColor> shades;

    if (rest.empty())
    {
        mesh.setMode(OF_PRIMITIVE_TRIANGLES);

        auto color = [](int r, int g, int b) {
            return ofFloatColor(r/255.0f, g/255.0f, b/255.0f, 1.0f);
        };

        auto vertex = [&](float x, float y, ofFloatColor c) {
            ofVec3f p(x,y,0);
            rest.push_back(p);
            shades.push_back(c);
            mesh.addVertex(p);
            mesh.addColor(c);
        };

        auto triangle = [&](ofVec3f a, ofVec3f b, ofVec3f c,
                            ofFloatColor shade) {
            vertex(a.x,a.y,shade);
            vertex(b.x,b.y,shade);
            vertex(c.x,c.y,shade);
        };

        const ofFloatColor fin = color(65,102,122);
        const ofFloatColor finLight = color(100,143,159);

        for (int side=-1; side<=1; side+=2)
        {
            // Swept pectoral fins.
            triangle(ofVec3f(.55f,side*.35f,0),
                     ofVec3f(-1.35f,side*1.75f,0),
                     ofVec3f(-.90f,side*.38f,0),fin);

            triangle(ofVec3f(.55f,side*.35f,0),
                     ofVec3f(-1.05f,side*1.45f,0),
                     ofVec3f(-.50f,side*.43f,0),finLight);

            // Small pelvic fins.
            triangle(ofVec3f(-1.7f,side*.25f,0),
                     ofVec3f(-2.45f,side*.70f,0),
                     ofVec3f(-2.30f,side*.12f,0),fin);

            // Tail silhouette.
            triangle(ofVec3f(-3.40f,0,0),
                     ofVec3f(-4.55f,side*.65f,0),
                     ofVec3f(-4.12f,0,0),finLight);
        }

        const float profile[][2] = {
            {-3.65f,.07f},{-3.20f,.12f},{-2.70f,.21f},
            {-2.10f,.33f},{-1.40f,.48f},{-.60f,.63f},
            {.20f,.67f},{.85f,.56f},{1.35f,.38f},{1.65f,.24f}
        };

        auto strip = [&](float x0,float w0,float x1,float w1) {
            auto shade = [&](float across) {
                float bright=1.0f-across*across;
                return ofFloatColor(
                    (58+76*bright)/255.0f,
                    (95+80*bright)/255.0f,
                    (115+75*bright)/255.0f,1);
            };

            for (int j=0;j<6;++j) {
                float a=-1.0f+j/3.0f;
                float b=a+1.0f/3.0f;

                vertex(x0,a*w0,shade(a));
                vertex(x1,a*w1,shade(a));
                vertex(x1,b*w1,shade(b));

                vertex(x0,a*w0,shade(a));
                vertex(x1,b*w1,shade(b));
                vertex(x0,b*w0,shade(b));
            }
        };

        for (int j=0;j<9;++j)
            strip(profile[j][0],profile[j][1],
                  profile[j+1][0],profile[j+1][1]);

        // Rounded hammerhead viewed directly from above.
        const float head[][2] = {
            {1.12f,.45f},{1.26f,.85f},{1.46f,1.08f},
            {1.78f,1.13f},{2.02f,1.04f},{2.17f,.82f},
            {2.22f,.20f}
        };

        for (int j=0;j<6;++j)
            strip(head[j][0],head[j][1],
                  head[j+1][0],head[j+1][1]);

        // Narrow dorsal ridge, rather than a side-view fin.
        triangle(ofVec3f(.10f,0,0),
                 ofVec3f(-1.05f,-.13f,0),
                 ofVec3f(-.80f,.13f,0),color(49,82,101));

        triangle(ofVec3f(.10f,0,0),
                 ofVec3f(-.80f,.02f,0),
                 ofVec3f(-.80f,.13f,0),color(158,189,196));

        auto dot = [&](float x,float y,float r,ofFloatColor c) {
            for(int k=0;k<12;++k) {
                float a=k*TWO_PI/12.0f;
                float b=(k+1)*TWO_PI/12.0f;

                triangle(ofVec3f(x,y,0),
                    ofVec3f(x+r*cos(a),y+r*sin(a),0),
                    ofVec3f(x+r*cos(b),y+r*sin(b),0),c);
            }
        };

        for(int side=-1;side<=1;side+=2) {
            dot(1.85f,side*.94f,.105f,color(12,24,32));
            dot(1.88f,side*.92f,.032f,color(228,239,237));

            for(int k=0;k<4;++k) {
                float x=.91f-k*.13f;
                float y=side*(.36f+k*.025f);

                triangle(
                    ofVec3f(x,y,0),
                    ofVec3f(x-.035f,y+side*.13f,0),
                    ofVec3f(x-.065f,y,0),color(52,85,103));
            }
        }
    }

    const float now=ofGetElapsedTimef();
    float dt=0;

    if(sharkVisualLastTime>=0)
        dt=ofClamp(now-sharkVisualLastTime,0.0f,.10f);
    else
        sharkVisualPhase=static_cast<float>(size)*1.73f;

    sharkVisualLastTime=now;

    float ratio=0;
    if(topSpeed>0)
        ratio=ofClamp(
            static_cast<float>(velocity.length()/topSpeed),
            0.0f,1.0f);

    // Integrate phase so speed changes do not jump the animation.
    sharkVisualPhase=fmod(
        sharkVisualPhase+dt*(2.0f+3.5f*ratio),TWO_PI);

    const float amplitude=.13f+.43f*ratio;
    const float red=isHunting ? 1.0f : (hunger>size ? .82f : 1.0f);
    const float green=isHunting ? .88f : (hunger>size ? .86f : 1.0f);
    const float blue=isHunting ? .84f : (hunger>size ? .89f : 1.0f);

    for(size_t i=0;i<rest.size();++i) {
        float x=rest[i].x;
        float y=rest[i].y;

        // Increasing movement toward the tail; head stays steady.
        float q=ofClamp((.75f-x)/5.30f,0.0f,1.0f);
        float wave=sharkVisualPhase-4.0f*q;
        float bend=amplitude*q*q*sin(wave);

        float slope=amplitude*
            (-2.0f*q*sin(wave)+4.0f*q*q*cos(wave))/5.30f;

        if(x>=.75f)
            slope=0;

        float tangent=atan(slope);

        mesh.setVertex(i,ofVec3f(
            x-y*sin(tangent),
            bend+y*cos(tangent),0));

        mesh.setColor(i,ofFloatColor(
            shades[i].r*red,
            shades[i].g*green,
            shades[i].b*blue,1));
    }

    ofPushStyle();
    ofPushMatrix();

    ofTranslate(projectorCoord);
    ofRotateDeg(swimDrawAngle);
    ofScale(size,size);
    ofEnableAlphaBlending();
    ofFill();

    // Layered shadow without an expensive blur pass.
    ofSetColor(0,0,0,14);
    ofDrawEllipse(-.7f,.28f,6.5f,2.35f);
    ofSetColor(0,0,0,22);
    ofDrawEllipse(-.65f,.23f,5.7f,1.7f);

    ofSetColor(255);
    mesh.draw();

    ofPopMatrix();
    ofPopStyle();
}


ofPoint Shark::wanderEffect()
{
	ofPoint velocityChange, desired;

	wandertheta += ofRandom(-change, change);     // Randomly change wander theta

	ofPoint front = velocity;
	front.normalize();
	front *= wanderD;
	ofPoint circleloc = location + front;

	float h = ofRadToDeg(atan2(front.y, front.x)); // Signed angle

	ofPoint circleOffSet = ofPoint(wanderR*cos(wandertheta + h), wanderR*sin(wandertheta + h));
	ofPoint target = circleloc + circleOffSet;

	desired = target - location;
	desired.normalize();
	desired *= topSpeed;

	velocityChange = desired - velocity;
	velocityChange.limit(maxVelocityChange);

	return velocityChange;
}

DangerousBOID::DangerousBOID(ofPoint loc, ofPoint vel, double sz)
{
	location = loc;
	velocity = vel;
	size = sz;
}

// DEEP_WATER_RAYS_V1
bool Vehicle::canSwimAt(const ofPoint& p) const
{
    const float radius=ofClamp(size*.55f,3.0f,7.0f);
    const float dx[]={0,radius,-radius,0,0};
    const float dy[]={0,0,0,radius,-radius};
    for(int i=0;i<5;++i) {
        const float x=p.x+dx[i],y=p.y+dy[i];
        if(!borders.inside(x,y)) return false;
        const float e=MagicSandAnimalTerrain::elevation(kinectProjector->elevationAtKinectCoord(x,y));
        if(!std::isfinite(e) || e>-20.0f) return false;
    }
    return true;
}

bool Vehicle::needsDeepWaterRescue() const
{
    return liveInWater && deepWaterStrandedSince>=0 &&
           ofGetElapsedTimef()-deepWaterStrandedSince>3.0f;
}

void Vehicle::updateDeepWaterAvoidance()
{
    const float now=ofGetElapsedTimef();
    const bool safe=canSwimAt(location);
    if(safe) deepWaterStrandedSince=-1;
    else if(deepWaterStrandedSince<0) deepWaterStrandedSince=now;
    beach=false;
    beachDist=1;
    beachSlope.set(0,0);
    ofVec2f forward(velocity.x,velocity.y);
    if(forward.lengthSquared()<.0001f)
        forward.set(cos(ofDegToRad(angle)),sin(ofDegToRad(angle)));
    forward.normalize();
    const float ahead=ofClamp(velocity.length()*40.0f+size,12.0f,85.0f);
    bool threat=!safe;
    for(int i=1;i<=3 && !threat;++i) {
        ofPoint p(location.x+forward.x*ahead*i/3.0f,
                  location.y+forward.y*ahead*i/3.0f);
        // The full footprint is checked again before moving.
        if(!borders.inside(p.x,p.y)) threat=true;
        else {
            float e=MagicSandAnimalTerrain::elevation(kinectProjector->elevationAtKinectCoord(p.x,p.y));
            threat=!std::isfinite(e) || e>-20.0f;
        }
    }
    if(threat) deepWaterAvoidUntil=now+.7f;
    if(!threat && now>=deepWaterAvoidUntil) return;
    beach=true;
    ofVec2f here(location.x,location.y);
    if(now>=deepWaterNextSearch || !deepWaterTargetReady ||
       here.distance(deepWaterTarget)<5.0f) {
        // Expensive searches are throttled, including failed searches.
        if(now>=deepWaterNextSearch) {
            deepWaterNextSearch=now+.35f;
            deepWaterTargetReady=false;
            const float radii[]={12,24,40,64,96};
            for(float radius:radii) {
                float best=-1.0e20f;
                for(int d=0;d<16;++d) {
                    const float a=TWO_PI*d/16.0f;
                    ofPoint candidate(location.x+cos(a)*radius,
                                      location.y+sin(a)*radius);
                    if(!canSwimAt(candidate)) continue;
                    if(safe) {
                        ofPoint mid((location.x+candidate.x)*.5f,
                                    (location.y+candidate.y)*.5f);
                        if(!canSwimAt(mid)) continue;
                    }
                    float score=cos(a)*forward.x+sin(a)*forward.y;
                    if(score>best) {
                        best=score;
                        deepWaterTarget.set(candidate.x,candidate.y);
                        deepWaterTargetReady=true;
                    }
                }
                if(deepWaterTargetReady) break;
            }
        }
    }
    if(deepWaterTargetReady) {
        beachSlope.set(deepWaterTarget.x-location.x,
                       deepWaterTarget.y-location.y);
    } else {
        // Turn back while waiting for a new search or habitat rescue.
        beachSlope.set(-forward.x,-forward.y);
    }
}

void Stingray::setup()
{
    size=ofRandom(5.5f,8.0f);
    topSpeed=size/18.0f;
    maxVelocityChange=topSpeed*.18f;
    maxRotation=12;
    minborderDist=20;
    internalBorders=borders;
    internalBorders.scaleFromCenter(
        std::max(.2f,(borders.width-20.0f)/borders.width),
        std::max(.2f,(borders.height-20.0f)/borders.height));
    angle=ofRandom(360.0f);
    velocity.set(cos(ofDegToRad(angle))*topSpeed,
                 sin(ofDegToRad(angle))*topSpeed);
    globalVelocityChange.set(0,0);
    swimHeadingReady=false;
    deepWaterTargetReady=false;
    deepWaterStrandedSince=-1;
    deepWaterNextSearch=0;
    deepWaterAvoidUntil=0;
    beach=false;
    border=false;
    wanderR=10;
    wanderD=40;
    change=.1f;
    rayPhase=ofRandom(TWO_PI);
    rayLastDraw=-1;
}

void Stingray::applyBehaviours()
{
    updateBeachDetection();
    ofPoint borderForce=bordersEffect();
    if(beach) return; // The shared swimming update takes over steering.
    if(border) { applyVelocityChange(borderForce*2.0f); return; }
    const float heading=ofDegToRad(angle)+
        .32f*sin(ofGetElapsedTimef()*.6f+rayPhase);
    ofPoint desired(cos(heading)*topSpeed,sin(heading)*topSpeed);
    ofPoint force=desired-velocity;
    force.limit(maxVelocityChange);
    applyVelocityChange(force);
}

void Stingray::draw()
{
    static ofMesh mesh;
    static std::vector<ofVec3f> rest;
    if(rest.empty()) {
        mesh.setMode(OF_PRIMITIVE_TRIANGLES);
        auto add=[&](float x,float y,float light) {
            ofVec3f p(x,y,0);
            rest.push_back(p);
            mesh.addVertex(p);
            mesh.addColor(ofFloatColor(
                .26f+.28f*light,.34f+.29f*light,.37f+.29f*light,1));
        };
        // Broad diamond-shaped disc, viewed directly from above.
        const float rows[][2]={
            {-1.65f,.08f},{-1.1f,.75f},{-.35f,1.75f},
            {.25f,1.50f},{.90f,.75f},{1.35f,.18f},{1.43f,.02f}
        };
        for(int r=0;r<6;++r) for(int j=0;j<8;++j) {
            float a=-1.0f+j*.25f,b=a+.25f;
            float x0=rows[r][0],x1=rows[r+1][0];
            float w0=rows[r][1],w1=rows[r+1][1];
            float l0=1-a*a,l1=1-b*b;
            add(x0,a*w0,l0);add(x1,a*w1,l0);add(x1,b*w1,l1);
            add(x0,a*w0,l0);add(x1,b*w1,l1);add(x0,b*w0,l1);
        }
        // Slender tapered whip tail.
        for(int j=0;j<14;++j) {
            float x0=-1.5f-j*.21f,x1=x0-.21f;
            float w0=.065f*(1-j/15.0f),w1=.065f*(1-(j+1)/15.0f);
            add(x0,-w0,.15f);add(x1,-w1,.15f);add(x1,w1,.35f);
            add(x0,-w0,.15f);add(x1,w1,.35f);add(x0,w0,.35f);
        }
    }
    const float now=ofGetElapsedTimef();
    if(rayLastDraw>=0) rayPhase+=ofClamp(now-rayLastDraw,0.0f,.1f)*3.4f;
    rayLastDraw=now;
    rayPhase=fmod(rayPhase,TWO_PI);
    for(size_t i=0;i<rest.size();++i) {
        float x=rest[i].x,y=rest[i].y;
        if(x<-1.65f) {
            float t=(-x-1.65f)/3.0f;
            y+=.20f*t*t*sin(rayPhase-t*3.5f);
        } else {
            float wing=fabs(y)/1.75f;
            y*=1.0f-.09f*wing*sin(rayPhase-x*1.7f-wing*2.0f);
            x+=.055f*wing*wing*cos(rayPhase-wing*2.0f);
        }
        mesh.setVertex(i,ofVec3f(x,y,0));
    }
    ofPushStyle();ofPushMatrix();
    ofTranslate(projectorCoord);
    ofRotateDeg(swimDrawAngle);
    ofScale(size,size);
    ofEnableAlphaBlending();ofFill();
    ofSetColor(0,0,0,20);
    ofDrawEllipse(-.15f,.25f,3.5f,3.2f);
    ofSetColor(255);mesh.draw();
    // Small paired eyes on the dorsal surface.
    ofSetColor(15,27,33);
    ofDrawEllipse(.85f,-.30f,.20f,.14f);
    ofDrawEllipse(.85f,.30f,.20f,.14f);
    ofSetColor(207,225,221);
    ofDrawCircle(.89f,-.30f,.025f);
    ofDrawCircle(.89f,.30f,.025f);
    ofPopMatrix();ofPopStyle();
}
