﻿#include "ParticleCollision.h"

namespace Engine
{
	ParticleCollision::ParticleCollision(Particle* particle1, Particle* particle2, float restitution, glm::vec3& normal) : particles{ particle1, particle2 },
		restitution(restitution), collisionNormal(normal)
	{}

	ParticleCollision::~ParticleCollision()
	{}

	/*
	Vc = -(·Pa - ·Pb)·(^(Pa - Pb)) 
	*/
	float ParticleCollision::CalculateSeparatingVelocity() const
	{
		glm::vec3 relVelocity = particles[0]->GetVelocity();
		relVelocity -= particles[1]->GetVelocity(); // Would be zero if the object is immovable
		return glm::dot(relVelocity, collisionNormal); // Dot product of the 2 vectors
	}

	void ParticleCollision::Resolve()
	{
		ResolveVelocity();
		ResolveInterpenetration();
	}

	/*
	Vc` = -cVc 
	Vs = (·Pa - ·Pb) · n^
	Impulse g = m(·p)
	Total change in velocity (·p`) = ·p + (1/m)g
	*/
	void ParticleCollision::ResolveVelocity()
	{
		// Check if its already separating or stationary
		float seperatingVelocity = CalculateSeparatingVelocity();
		if (seperatingVelocity > 0)
			return;

		// Check if both have infinite mass if they do no impulse generated
		float totalInverseMass = particles[0]->GetInverseMass() + particles[1]->GetInverseMass();
		if (totalInverseMass <= 0)
			return;

		// Calculate the new separating velocity and factor in the restitution
		float newSeperatingVelocity = -seperatingVelocity * restitution;

		// This is a bit of a hack but should work for the moment
		// Check for velocity caused by acceleration in the direction of the closing velocity
		glm::vec3 accelerationBuildUp = particles[0]->GetAcceleration() - particles[1]->GetAcceleration();
		float accelerationCausedSepVelocity = glm::dot(accelerationBuildUp, collisionNormal * GameTime::deltaTime);
		if (accelerationCausedSepVelocity < 0)
		{
			// Subtract the acceleration in the direction of the collision normal from the separating velocity
			newSeperatingVelocity += accelerationCausedSepVelocity * restitution;

			// We don't want a negative new separating velocity
			if (newSeperatingVelocity < 0)
				newSeperatingVelocity = 0;
		}

		float deltaVelocity = newSeperatingVelocity - seperatingVelocity;
		float impulse = deltaVelocity / totalInverseMass;
		glm::vec3 impulsePerMass = collisionNormal * impulse;

		// Apply the impulses
		if (!particles[0]->GetIsInfiniteMass())
			particles[0]->GetVelocity() += impulsePerMass * particles[0]->GetInverseMass();

		// Apply opposite impulse
		if (!particles[1]->GetIsInfiniteMass())
			particles[1]->GetVelocity() += impulsePerMass * -particles[1]->GetInverseMass();
	}

	/*
	ΔPa + ΔPb = d.
	ΔP is the scalar distance that object awill bemoved.
	MaΔPa = MbsΔPb, ΔPa = (Mb / Mb + Ma ) dn.
	*/
	void ParticleCollision::ResolveInterpenetration()
	{
		// If there is no proper penetration
		if (penetration <= 0)
			return;

		// Check that both particles are not infinite mass
		float totalInverseMass = particles[0]->GetInverseMass() + particles[1]->GetInverseMass();
		if (totalInverseMass <= 0)
			return;

		// Find the movement vector according to penetration and inverse mass
		glm::vec3 moveVector = collisionNormal * (-penetration / totalInverseMass);

		// Apply this movement vector
		if (particles[0]->GetInverseMass())
			particles[0]->GetPosition() += moveVector * particles[0]->GetInverseMass();

		if (particles[1]->GetInverseMass())
			particles[1]->GetPosition() += moveVector * -particles[1]->GetInverseMass();
	}

}