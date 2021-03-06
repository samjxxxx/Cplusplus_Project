﻿#include <memory.h>
#include <assert.h>

#include "contacts.h"

using namespace Kawaii;

void Contact::setBodyData(RigidBody* one, RigidBody* two,
    real friction, real restitution)
{
    Contact::body[0] = one;
    Contact::body[1] = two;
    Contact::friction = friction;
    Contact::restitution = restitution;
}

/*
we need to calculate each of the
three bits of data: the contact basis, the relative position, and the relative velocity
*/
void Contact::calculateInternals(real duration)
{
    // Check if the first object is NULL, and swap if it is.
    if(!body[0]) swapBodies();
    assert(body[0]);

    //Calculate the set of axis at the contact point.
    calculateContactBasis();

    //Store relative position of the contact to each body
    relativeContactPosition[0] = contactPoint - body[0]->getPosition();
    if (body[1])
    {
        relativeContactPosition[1] = contactPoint - body[1]->getPosition();
    }
    
    //Find the relative velocity of the bodies at the contact point
    contactVelocity = calculateLocalVelocity(0, duration);

    if (body[1])
    {
        contactVelocity -= calculateLocalVelocity(1, duration);
    }

    //calculate the desirewd changed in velocity for resolution
    calculateDesiredDeltaVelocity(duration);
}

/*
Swaps the bodies in the current contact, so body 0 is at body 1 and
vice versa. This also changes the direction of the contact normal,
but doesn't update any calculated internal data.
*/
void Contact::swapBodies()
{
    contactNormal *= -1;

    RigidBody* temp = body[0];
    body[0] = body[1];
    body[1] = temp;
}

void Contact::matchAwakeState()
{
    // Collisions with the world never cause a body to wake up.
    if (!body[1]) return;

    bool body0awake = body[0]->getAwake();
    bool body1awake = body[1]->getAwake();

    // Wake up only the sleeping one
    if (body0awake ^ body1awake)
    {
        if (body0awake) body[1]->setAwake();
        else body[0]->setAwake();
    }
}

/*
The relative velocity we are interesting in is the total closing velocity of both objs at the contact point.
x values will give the velocity direction of the contact normal.
y and z value will give the amount of the sliding that taking place at contact.
Velocity at point `Qrel = Ò x Qrel + `p

Qrel is the position of the point we are interesting in, relative to the objs center of mass.
p = position of objs center of mass(`p is lineal velocity of the whole obj) Ò angular velocity.

*/
Vector3 Contact::calculateLocalVelocity(unsigned bodyIndex, real duration)
{
    RigidBody* thisBody = body[bodyIndex];

    //Work out the velocity of the contact point
    Vector3 velocity = thisBody->getRotation() % relativeContactPosition[bodyIndex];
    velocity += thisBody->getVelocity();

    // Turn the velocity into contact-coordinates.
    Vector3 contactVelocity = contactToWorld.transformTranspose(velocity);

    //Calculate the ammount of the velocity that is due to forces without relation
    Vector3 accVelocity = thisBody->getLastFrameAcceleration() * duration;

    // Calculate the velocity in contact-coordinates.
    accVelocity = contactToWorld.transformTranspose(accVelocity);

    // We ignore any component of acceleration in the contact normal
    // direction, we are only interested in planar acceleration
    accVelocity.x = 0;

    // Add the planar velocities - if there's enough friction they will
   // be removed during velocity resolution
    contactVelocity += accVelocity;

    return contactVelocity;
}

/*
This is stored as a 3x3 matrix, where each vector is a column.
The x direction is generated from the contact normal, and the y and z directions 
are set so they are at right angles to it.
*/
void Contact::calculateContactBasis()
{
    Vector3 contactTangent[2];

    //Check whether the Z - axis is nearer to the X or Y axis
    if (real_abs(contactNormal.x) > real_abs(contactNormal.y))
    {
        //Scaling dactor ro ensure the result are normalised
        const real s = (real)1.0f / real_sqrt(contactNormal.z * contactNormal.z +
            contactNormal.x * contactNormal.x);

        // The new X-axis is at right angles to the world Y-axis
        contactTangent[0].x = contactNormal.z * s;
        contactTangent[0].y = 0;
        contactTangent[0].z = -contactNormal.x * s;

        // The new Y-axis is at right angles to the new X- and Z- axes
        contactTangent[1].x = contactNormal.y * contactTangent[0].x;
        contactTangent[1].y = contactNormal.z * contactTangent[0].x - contactNormal.x * contactTangent[0].z;
        contactTangent[1].z = -contactNormal.y * contactTangent[0].x;
    }
    else
    {
        // Scaling factor to ensure the results are normalised
        const real s = (real)1.0 / real_sqrt(contactNormal.z * contactNormal.z +
            contactNormal.y * contactNormal.y);
        // The new X-axis is at right angles to the world X-axis
        contactTangent[0].x = 0;
        contactTangent[0].y = -contactNormal.z * s;
        contactTangent[0].z = contactNormal.y * s;

        // The new Y-axis is at right angles to the new X- and Z- axes
        contactTangent[1].x = contactNormal.y * contactTangent[0].z -
            contactNormal.z * contactTangent[0].y;
        contactTangent[1].y = -contactNormal.x * contactTangent[0].z;
        contactTangent[1].z = contactNormal.x * contactTangent[0].y;
    }

    // Make a matrix from the three vectors.
    contactToWorld.setComponents(contactNormal, contactTangent[0], contactTangent[1]);
}

/*
For contacts with 2 objs invilved we have 4 value : 
- vel caused by lineal and angunar motion for each objs
For the contact we only have Rigid bodie.

The Linear Component : holds only for the linear component of velocity
 Δ`Pd = m(a)^-1 + m(b)^-1

 The Angular Component
 Qrel = pos of the contact relative to the origin of a obj.
Qrel = q - p
u = Qrel x ^d <(TorquePerUnitImpulse = relativeContactPosition % normal )>
ΔÒ = I^-1 · u <[ rotationPerUnitImpulse = inverseInertiaTensor.transform(torquePerUnitImpulse)]>
the total velocity of a point
`q = Ò X Qrel <[velocityPerUnitImpulse = rotationPerUnitImpulse % TorquePerUnitImpulse]>

Velocity, velocityPerUnitImpulseContact(transformTranspose(velocityPerUnitImpulse);))

The rotation induced velocty of a point (`q) depend on its position relative to the origin of the obj (q - p)
and on the objs angular velocity (Ò).
The result will be the velocity caused by rotation per unit impulse.
The velocity is in world space, we interesting along the contact normal.

-VELOCITY CHANGE BY IMPULSE : Impulses cause a change in velocity, calculate the impulse at the collision
-IMPULSE CHANGE BY VELOCITY : CALCULATING THE DESIRED VELOCITY CHANGE, CALCULATING THE IMPULSE, APPLYING THE IMPULSE
*/
inline
Vector3 Contact::calculateFrictionlessImpulse(Matrix3* inverseInertiaTensor)
{
    Vector3 impulseContact;

    // Build a vector that shows the change in velocity in
    // world space for a unit impulse in the direction of the contact
    // normal.
    Vector3 deltaVelWorld = relativeContactPosition[0] % contactNormal;
    deltaVelWorld = inverseInertiaTensor[0].transform(deltaVelWorld);
    deltaVelWorld = deltaVelWorld % relativeContactPosition[0];

    // Work out the change in velocity in contact coordiantes.
    real deltaVelocity = deltaVelWorld * contactNormal;

    // Add the linear component of velocity change
    deltaVelocity += body[0]->getInverseMass();

    // Check if we need to the second body's data
    if (body[1])
    {
        // Go through the same transformation sequence again
        Vector3 deltaVelWorld = relativeContactPosition[1] % contactNormal;
        deltaVelWorld = inverseInertiaTensor[1].transform(deltaVelWorld);
        deltaVelWorld = deltaVelWorld % relativeContactPosition[1];

        // Add the change in velocity due to rotation
        deltaVelocity += deltaVelWorld * contactNormal;

        // Add the change in velocity due to linear motion
        deltaVelocity += body[1]->getInverseMass();
    }

    // Calculate the required size of the impulse
    impulseContact.x = desiredDeltaVelocity / deltaVelocity;
    impulseContact.y = 0;
    impulseContact.z = 0;
    return impulseContact;
}


/*
CALCULATING THE DESIRED VELOCITY CHANGE :
Calculating the Closing Velocity : velocity has both a linear and an angular component. To calculate
the total velocity of one object at the contact point we need both.
 We can retrieve the linear velocity from an object directly, it is stored in the rigid body.

V`s = -CVs => ΔVs = -Vs - CVs = (-1+C)Vs
real deltaVelocity = -contactVelocity.x * (1 + restitution);

We need remove all existing closing velocity at the contact, and keep going so the final velocity c times its 
original value but in the oposite direction.
If the coefficient of restitution, c, is zero, then the change in velocity will be sufficient
to remove all the existing closing velocity but no more.
*/
void Contact::calculateDesiredDeltaVelocity(real duration)
{
    const static real velocityLimit = (real)0.25f;

    //Calculate the acceleration induced velocity accumulate this frame
    real velocityFromAcc = 0;

    if (body[0]->getAwake())
    {
        velocityFromAcc += body[0]->getLastFrameAcceleration() * duration * contactNormal;
    }

    if (body[1] && body[1]->getAwake())
    {
        velocityFromAcc -=  body[1]->getLastFrameAcceleration() * duration * contactNormal;
    }
    // If the velocity is very slow, limit the restitution
    real thisRestitution = restitution;
    if (real_abs(contactVelocity.x) < velocityLimit)
    {
        thisRestitution = (real)0.0f;
    }

    // Combine the bounce velocity with the removed acceleration velocity.
    desiredDeltaVelocity = -contactVelocity.x - thisRestitution * (contactVelocity.x - velocityFromAcc);
}

/*
Calculating and  Applying the impulse
In contact coordenade the contact normal is in X axis G(contact) = [g ,0 , 0]. G is the impulse
convert out of contact coordinates into world coordinates.<[gworld = Mgcontact]>, Convert impulse to world coordinates.

`p = g / m velocity change
rotation change = ΔÒ = I^-1 · u 
u = qrel × g

*/
void Contact::applyVelocityChange(Vector3 velocityChange[2], Vector3 rotationChange[2])
{
    //Get hold of the inverse mass and inverse inertia tensor, both in world coordinates
    Matrix3 inverseInertiaTensor[2];
    body[0]->getInverseInertiaTensorWorld(&inverseInertiaTensor[0]);
    if (body[1])
    {
        body[1]->getInverseInertiaTensorWorld(&inverseInertiaTensor[1]);
    }

    // We will calculate the impulse for each contact axis
    Vector3 impulseContact;

    if (friction == (real)0.0)
    {
        impulseContact = calculateFrictionlessImpulse(inverseInertiaTensor);
    }
    else
    {
        // Otherwise we may have impulses that aren't in the direction of the
        // contact, so we need the more complex version.
        //TODO
    }
    //Converte impulse to wolrd coordinates
    Vector3 impulse = contactToWorld.transform(impulseContact);

    // Split in the impulse into linear and rotational components
    //u = qrel × g (g = impulse)
    //ΔÒ = I^-1 · u 
    // `p = g / m
    Vector3 impulsiveTorque = relativeContactPosition[0] % impulse;
    rotationChange[0] = inverseInertiaTensor[0].transform(impulsiveTorque);
    velocityChange[0].clear();
    velocityChange[0].addScaledVector(impulse, body[0]->getInverseMass());

    // Apply the changes
    body[0]->addVelocity(velocityChange[0]);
    body[0]->addRotation(rotationChange[0]);

    if (body[1])
    {
        // Work out body one's linear and angular changes
        Vector3 impulsiveTorque = impulse % relativeContactPosition[1];
        rotationChange[1] = inverseInertiaTensor[1].transform(impulsiveTorque);
        velocityChange[1].clear();
        // opposite direction respective to 1 obj.
        velocityChange[1].addScaledVector(impulse, -body[1]->getInverseMass());

        // And apply them.
        body[1]->addVelocity(velocityChange[1]);
        body[1]->addRotation(rotationChange[1]);
    }
}

/*
Ìnterpenetration : IMPLEMENTING NONLINEAR PROJECTION 
Calculating the Components : 
Inertial : the resistance of an object TO BEING MOVING
inertia will have a linear component and a rotational component.
The linear component of inertia = INVERSE MASS
The angular component previus used.

Applying the Movement 
Angular motion : 1 calculate the rotation needed to move the contact point by one unit.
2 multiply this by number of unit needed
3 apply the rotation to the orientation quartenion.


*/
void Contact::applyPositionChange(Vector3 linearChange[2], Vector3 angularChange[2], real penetration)
{
    const real angularLimit = (real)0.2f;
    real angularMove[2];
    real linearMove[2];

    real totalInertia = 0;
    real linearInertia[2];
    real angularInertia[2];
    // We need to work out the inertia of each object in the direction
    // of the contact normal, due to angular inertia only.
    for (unsigned i = 0; i < 2; i++)
    {
        if (body[i])
        {
            Matrix3 inverseInertiaTensor;
            body[i]->getInverseInertiaTensorWorld(&inverseInertiaTensor);

            //Use the same procedure as for calculating frictionless
             // velocity change to work out the angular inertia.
            Vector3 angularInertiaWorld = relativeContactPosition[i] % contactNormal;
            angularInertiaWorld = inverseInertiaTensor.transform(angularInertiaWorld);
            angularInertiaWorld = angularInertiaWorld % relativeContactPosition[i];
            angularInertia[i] = angularInertiaWorld * contactNormal;

            // The linear component is simply the inverse mass
            linearInertia[i] = body[i]->getInverseMass();

            //keep track of total inertial from all component
            totalInertia += linearInertia[i] + angularInertia[i];
            // We break the loop here so that the totalInertia value is
            // completely calculated (by both iterations) before
            // continuing.
        }
    }

    // Loop through again calculating and applying the changes
    for (unsigned i = 0; i < 2; i++) if (body[i])
    {
        //TODO
    }
}

/*
The contact resolution routine that the whole set of the collison and duration of the frame
and performs the resolution in 3 step:
    1- calculate the internal data for each contact.
    2- passes the contacts penetration resolver.
    3- velocity resolver.
*/
void ContactResolver::resolveContacts(Contact* contactArray, unsigned numContacts, real duration)
{
    // Make sure we have something to do.
    if (numContacts == 0) return;

    //Prepare for contact processing
    prepareContacts(contactArray, numContacts, duration);

    // Resolve the interpenetration problems with the contacts.
    adjustPositions(contactArray, numContacts, duration);

    // Resolve the velocity problems with the contacts.
    adjustVelocities(contactArray, numContacts, duration);
}

/*
We performing a penetration resolution step as well as a velocity resolution step for each contact.
It usefull to calculate information that both steps need in 1 central location.
In the first category are two bits of data:
- the basis matrix for contact point, calculated in the calculateContactBasis method, is called contactToWorld.
-The position of the contact is relative to each object. relativeContactPosition
*/
void ContactResolver::prepareContacts(Contact* contacts, unsigned numContacts, real duration)
{
    // Generate contact velocity and axis information.
    Contact* lastContact = contacts + numContacts;
    for (Contact* contact = contacts; contact < lastContact; contact++)
    {
        contact->calculateInternals(duration);
    }
}

/*
RESOLVING PENETRATION
At each iteration we search throght to find the collision with the deepest penetration value.
This is handled through its "applyPositionChange" method in the normal way. The process is then reapeated up
 to so maximum number of iteration.

--Iterative Algorithm Implemented
Looks throght the whole list of contacts at each iteration. Too complex to be run for each iteration of resolution algorith.

--Updating Penetration Depths
When the penetration for collision is resolved, only 1 or 2 objs can be moved.
After resolving the penetration, we keep track of the lineal and angular motion of each objs.
To calculate the new penetration value we calculate the new position of the relative contact point for each object,
based on lineal and angular movements. The penetration value is adjusted based on the new position
of these two points.
if they have moved apart the nthe penetration will be less.

Finally we need some mechanism for storing the adjustments made in the "applyPositionChange" method for use in this update.
finding the worst penetration, resoving it and then updating the remaing contacts.
*/

void ContactResolver::adjustPositions(Contact* contacts, unsigned numContacts, real duration)
{
    unsigned i, index;
    Vector3 linearChange[2], angularChange[2];
    real max;
    Vector3 deltaPosition;

    // iteratively resolve interpenetrations in order of severity.
    positionIterationsUsed = 0;
    while (positionIterationsUsed < positionIterations)
    {
        // Find biggest penetration
        max = positionEpsilon;
        index = numContacts;
        for (int i = 0; i < numContacts; i++)
        {
            if (contacts[i].penetration > max)
            {
                max = contacts[i].penetration;
                index = i;
            }
        }
        if (index == numContacts) break;

        //Match the awake state at the contact
        contacts[index].matchAwakeState();

        // Resolve the penetration.
        contacts[index].applyPositionChange(linearChange, angularChange, max);

        // Again this action may have changed the penetration of other
        // bodies, so we update contacts.
        for (int i = 0; i < numContacts; i++)
        {
            // Check each body in the contact
            for (unsigned b = 0; b < 2; b++) if (contacts[i].body[b])
            {
                //Check for a match with each body in the newly
                //resolver contact
                for (unsigned d = 0; d < 2; d++)
                {
                    if (contacts[i].body[b] == contacts[index].body[d])
                    {
                        deltaPosition = linearChange[d] + angularChange[d].vectorProduct(contacts[i].relativeContactPosition[b]);

                        //The sign of the change is positive if we're dealing with 2 bodyin a contact.
                        contacts[i].penetration += deltaPosition.scalarProduct(contacts[i].contactNormal) * (b ? 1 : -1);
                    }
                }
            }
        }
        positionIterationsUsed++;
    }
}

/*
RESOLVING VELOCITY
The algorith run with iteration, at each iteration finds the collison with the greates closing velocity.
If there is no collision with a closing velocity, then the algorithm
can terminate. If there is a collision, then it is resolved in isolation. Other contacts are then
update based on the changed that were made.
*/

void ContactResolver::adjustVelocities(Contact* c, unsigned numContacts, real duration)
{
    //TODO
    Vector3 velocityChange[2], rotationChange[2];
    Vector3 deltaVel;

    velocityIterationsUsed = 0;
    while (velocityIterationsUsed < velocityIterations)
    {
        //Find the contact with maximum magnitude of probable velocity chenge.
        real max = velocityEpsilon;
        unsigned index = numContacts;
        for (unsigned i = 0; i < numContacts; i++)
        {
            if (c[i].desiredDeltaVelocity > max)
            {
                max = c[i].desiredDeltaVelocity;
                index = i;
            }
        }
        if (index == numContacts) break;

        // Match the awake state at the contact
        c[index].matchAwakeState();

        //Do the resolution on the contact come out top
        c[index].applyVelocityChange(velocityChange, rotationChange);

        //With velocity chnaged in 2 bodies, update of contact velocities mean that some of the 
        //relative closing velocities need recomputing
        for (unsigned i = 0; i < numContacts; i++)
        {
            //check each bodies in the contact
            for (unsigned b = 0; b < 2; b++) if (c[i].body[b])
            {
                // Check for a match with each body in the newly
                // resolved contact
                for (unsigned d = 0; d < 2; d++)
                {
                    if (c[i].body[b] == c[index].body[d])
                    {
                        deltaVel = velocityChange[d] + rotationChange[d].vectorProduct(c[i].relativeContactPosition[b]);
                        // The sign of the change is negative if we're dealing
                        // with the second body in a contact.
                        c[i].contactVelocity += c[i].contactToWorld.transformTranspose(deltaVel) * (b ? -1 : 1);
                        c[i].calculateDesiredDeltaVelocity(duration);
                    }
                }
            }
        }
        velocityIterationsUsed++;
    }
}


/*
Friction : is the force generate when one obj moves or try to move in contact with another. (Static and dynamic)
Rolled together into a generic friction value.
Static friction is a force that stops an object moving when it is stationary (sometime called stiction).
|Fstatic| <= μ(static)|r|
r = reaction force in the direction of contact normal
|Fstatic| = friction force generate
μ static = coeff os static friction, encapsule all material property.
Expression of Fstatic = { -Fplanar , ^Fplanar - μ(static)} Fplanar = total force on the obj in the plane of contact only
r = -f · ^d  <[^d = contact normal,  f = force exerted]>
Fplanar = f + r
Dynamic Friction（kinetic friction）
fdynamic = −^Vplanar μ(dynamic) |r|
acting in the opposite direction to the planar force.
types = Isotropic friction has the same coefficient in all directions. Anisotropic friction can have different coefficients in different directions.
Friction as Impulses g = ft (g = impulse, f = force), the equation require normal reaction force
f = Δvmt
Combine impulse and friction equation
g(max) = Δg(normal) μ
that these impulse values are scalar: this tells us the total impulse we can apply with static friction.
Velocity Resolution Algorithm : an impulse in one direction can increase the velocity of the contact in a different direction
1 - We work in a set of coordenates that are realtive to the contact. We create transform matrix to convert into and out of new set coordinates.
Turning impulse to a torque
2 - Work out in changed velocity of the contact point on each obj per unit impulse. impulse will cause linear and angular motion,
it will tranform a vetor (impulse) to another vector(resulting velocity)->matrix
3 - know the velocity change we want to see, so we invert the result of the last stage to find the impulse needed to generate any given velocity.
find the inverse of matrix.
4 - Work out the separating velocity at the contact point shoul be.
5 - From the change in velocity, we can calculate the impulse that must be genrate
6 - We split the impulse into its linear and angular components and apply to each obj.
Note : that only the impulseToTorque and inverseInertiaTensormatrices will change for each body.contactToWorld are same each case
Velocity from Linear Motion Δ`p = m^-1 g
We are trying combineing one matrix with lineal and angular componet of velocity.
τ = Pf x f work the entire basis matrix through the same series of steps:
// This was a cross-product.
Matrix3 torquePerUnitImpulse = impulseToTorque * contactToWorld;
// This was a vector transformed by the tensor matrix; now it’s
Matrix3 rotationPerUnitImpulse = inverseInertiaTensor * torquePerUnitImpulse;
This was the reverse cross-product, so we’ll need to multiply the
// result by -1.
Matrix3 velocityPerUnitImpulse = rotationPerUnitImpulse * impulseToTorque;
velocityPerUnitImpulse *= -1;
// Finally, convert the result into contact coordinates.
Matrix3 velocityPerUnitImpulseContact = contactToWorld.transpose() * velocityPerUnitImpulse;
*/
inline
Vector3 Contact::calculateFrictionImpulse(Matrix3* inverseInertiaTensor)
{
    Vector3 impulseContact;
    real inverseMass = body[0]->getInverseMass();

    //The equivalent of a cross product in matrices is multiplication by a skew symmetric matrix (1)
    Matrix3 impulseToTorque;
    impulseToTorque.setSkewSymmetric(relativeContactPosition[0]);

    //Build the matrix convert contact impulse to change in velocity in wolrd coordenates
    Matrix3 deltaVelWorld = impulseToTorque;
    deltaVelWorld *= inverseInertiaTensor[0];
    deltaVelWorld *= impulseToTorque;
    //This was reverse croos product , we will need to multiply by -1
    deltaVelWorld *= -1;

    // Check if we need to add body two's data
    if (body[1])
    {
        impulseToTorque.setSkewSymmetric(relativeContactPosition[1]);
        // Calculate the velocity change matrix
        Matrix3 deltaVelWorld2 = impulseToTorque;
        deltaVelWorld2 *= inverseInertiaTensor[1];
        deltaVelWorld2 *= impulseToTorque;
        deltaVelWorld2 *= -1;

        deltaVelWorld += deltaVelWorld2;
        // Add to the inverse mass
        inverseMass += body[1]->getInverseMass();
    }

    // Perform a change of basis to convert into contact coordinates(matrix transform impulse to velocity contact coordinates).
    //we change the basis of a matrix by BMB^-1, andM is thematrix being transformed BMB^t
    Matrix3 deltaVelocity = contactToWorld.transpose();
    deltaVelocity *= deltaVelWorld;
    deltaVelocity *= contactToWorld;

    // Add in the linear velocity change
    deltaVelocity.data[0] += inverseMass;
    deltaVelocity.data[4] += inverseMass;
    deltaVelocity.data[8] += inverseMass;

    // Invert to get the impulse needed per unit velocity
    Matrix3 impulseMatrix = deltaVelocity.inverse();

    // Find the target velocities to kill, closing velocity is killed by applying a small impulse
    Vector3 velKill(desiredDeltaVelocity, -contactVelocity.y, -contactVelocity.z);

    // Find the impulse to kill target velocities
    impulseContact = impulseMatrix.transform(velKill);

    // Check for exceeding friction
    real planarImpulse = real_sqrt(
        impulseContact.y * impulseContact.y +
        impulseContact.z * impulseContact.z);
    // Check that we’re within the limit of static friction.
    if (planarImpulse > impulseContact.x * friction)
    {
        // Handle as dynamic friction. Dynamic friction can be handled by scaling 
        //the Y and Z component pf impulse,divide planarImpulse scales the Y and Z components so they have a unit size
        impulseContact.y /= planarImpulse;
        impulseContact.z /= planarImpulse;

        impulseContact.x = deltaVelocity.data[0] +
            deltaVelocity.data[1] * friction * impulseContact.y +
            deltaVelocity.data[2] * friction * impulseContact.z;

        //Multiplying the direction by size gives new values for each component.
        impulseContact.x = desiredDeltaVelocity / impulseContact.x;
        impulseContact.y *= friction * impulseContact.x;
        impulseContact.z *= friction * impulseContact.x;
    }
    return impulseContact;
}