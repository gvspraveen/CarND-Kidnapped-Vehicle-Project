/*
 * particle_filter.cpp
 *
 *  Created on: Dec 12, 2016
 *      Author: Tiffany Huang
 */

#include <random>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <math.h> 
#include <iostream>
#include <sstream>
#include <string>
#include <iterator>

#include "particle_filter.h"

using namespace std;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
	// TODO: Set the number of particles. Initialize all particles to first position (based on estimates of 
	//   x, y, theta and their uncertainties from GPS) and all weights to 1. 
	// Add random Gaussian noise to each particle.
	// NOTE: Consult particle_filter.h for more information about this method (and others in this file).

  // cout << "Start init \n";

  // Setting num of particles.
  num_particles = 50;
  // lower threshold for yaw rate. to avoid division by zero
  eps = 0.00001;
  default_random_engine gen;

  // Normal distribution for state space - x, y, theta
  normal_distribution<double> dist_x(x, std[0]);
  normal_distribution<double> dist_y(y, std[1]);
  normal_distribution<double> dist_theta(theta, std[2]);

  for (int i = 0; i < num_particles; i++) {
     Particle new_p;
     new_p.id = i;
     new_p.theta = theta;
     new_p.weight = 1.0;

     // add noise
     new_p.x = dist_x(gen);
     new_p.y = dist_y(gen);
     new_p.theta = dist_theta(gen);

//     new_p.x = x;
//     new_p.y = y;
//     new_p.theta = theta;

     particles.push_back(new_p);
     weights.push_back(new_p.weight);
  }
  is_initialized = true;
  // cout << "End init \n";
}

void ParticleFilter::prediction(double delta_t, double std_pos[], double velocity, double yaw_rate) {
	// TODO: Add measurements to each particle and add random Gaussian noise.
	// NOTE: When adding noise you may find std::normal_distribution and std::default_random_engine useful.
	//  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
	//  http://www.cplusplus.com/reference/random/default_random_engine/
  default_random_engine gen;
  double std_x = std_pos[0];
  double std_y = std_pos[1];
  double std_theta = std_pos[2];

  normal_distribution<double> dist_x(0, std_x);
  normal_distribution<double> dist_y(0, std_y);
  normal_distribution<double> dist_theta(0, std_theta);

  // Calculate new state for each particle
  for (int i = 0; i < num_particles; i++) {
    double theta = particles[i].theta;
    if ( fabs(yaw_rate) < eps ) { // When yaw is not changing.
      particles[i].x += velocity * delta_t * cos( theta );
      particles[i].y += velocity * delta_t * sin( theta );
    } else {
      particles[i].x += velocity / yaw_rate * ( sin( theta + yaw_rate * delta_t ) - sin( theta ) );
      particles[i].y += velocity / yaw_rate * ( cos( theta ) - cos( theta + yaw_rate * delta_t ) );
      particles[i].theta += yaw_rate * delta_t;
    }

    // Adding noise.
    particles[i].x += dist_x(gen);
    particles[i].y += dist_y(gen);
    particles[i].theta += dist_theta(gen);
  }

  // cout << "End of prediction \n";
}

void ParticleFilter::dataAssociation(std::vector<LandmarkObs> predicted, std::vector<LandmarkObs>& observations) {
	// TODO: Find the predicted measurement that is closest to each observed measurement and assign the 
	//   observed measurement to this particular landmark.
	// NOTE: this method will NOT be called by the grading code. But you will probably find it useful to 
	//   implement this method and use it as a helper during the updateWeights phase.
  // cout << "Start of dataAssociation \n";
  for (unsigned int i = 0; i < observations.size(); i++) {
      double min_distance = numeric_limits<double>::max();
      int landmark_id;
      for (unsigned int j = 0; j < predicted.size(); j++) {
        double obs_distance = dist(observations[i].x, observations[i].y,  predicted[j].x, predicted[j].y);
        if (obs_distance < min_distance) {
          min_distance = obs_distance;
          landmark_id = predicted[j].id;
        }
      }
      observations[i].id = landmark_id;
  }
  // cout << "End of dataAssociation \n";

}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], 
		const std::vector<LandmarkObs> &observations, const Map &map_landmarks) {
	// TODO: Update the weights of each particle using a mult-variate Gaussian distribution. You can read
	//   more about this distribution here: https://en.wikipedia.org/wiki/Multivariate_normal_distribution
	// NOTE: The observations are given in the VEHICLE'S coordinate system. Your particles are located
	//   according to the MAP'S coordinate system. You will need to transform between the two systems.
	//   Keep in mind that this transformation requires both rotation AND translation (but no scaling).
	//   The following is a good resource for the theory:
	//   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
	//   and the following is a good resource for the actual equation to implement (look at equation 
	//   3.33
	//   http://planning.cs.uiuc.edu/node99.html
  // cout << "Started updateWeights";

  for (unsigned int i = 0; i < num_particles; i ++) {
      double px = particles[i].x;
      double py = particles[i].y;
      double ptheta = particles[i].theta;

      // Get a listed of predicted observations for this particle
      vector<LandmarkObs> predicted_observations;

      // For each map landmark we determine if the landmark is within sensor range for the particle
      for (unsigned int j = 0; j < map_landmarks.landmark_list.size(); j++) {
          double map_x = map_landmarks.landmark_list[j].x_f;
          double map_y = map_landmarks.landmark_list[j].y_f;
          int map_id = map_landmarks.landmark_list[j].id_i;

          if ((fabs((px - map_x)) <= sensor_range) && (fabs((py - map_y)) <= sensor_range)) {
            predicted_observations.push_back(LandmarkObs {map_id, map_x, map_y});
          }
      }

      // Transform observations into map coordinates
      vector<LandmarkObs> transformed_observations;

      for (unsigned int j = 0; j < observations.size(); j++) {
        LandmarkObs transformed;
        transformed.id = observations[j].id;
        transformed.x = px + (cos(ptheta) * observations[j].x) - (sin(ptheta) * observations[j].y);
        transformed.y = py + (sin(ptheta) * observations[j].x) + (cos(ptheta) * observations[j].y);
        transformed_observations.push_back(transformed);
      }

      // Associate transformed observations to predicated landmarks
      dataAssociation(predicted_observations, transformed_observations);

      particles[i].weight = 1.0;
      weights[i] = 1.0;
      for (unsigned int j = 0; j < transformed_observations.size(); j++) {
        double sigma_x = std_landmark[0];
        double sigma_y = std_landmark[1];

        double predicted_x, predicted_y;
        double obs_x = transformed_observations[j].x;
        double obs_y = transformed_observations[j].y;
        int obs_landmark_id = transformed_observations[j].id;

        for (unsigned int k = 0; k < predicted_observations.size(); k++) {
          if (predicted_observations[k].id == obs_landmark_id) {
            predicted_x = predicted_observations[k].x;
            predicted_y = predicted_observations[k].y;
          }
        }



        double dX = obs_x - predicted_x;
        double dY = obs_y - predicted_y;

        double weight_prob = ( 1/(2*M_PI*sigma_x*sigma_y)) * exp(
            -(
                (pow(dX, 2)/(2*pow(sigma_x, 2)))
                +
                (pow(dY, 2)/(2*pow(sigma_y, 2)) )
            )
        );
        particles[i].weight *= weight_prob;
        weights[i] *= weight_prob;
      }

  }

//
//  for (int i = 0; i < particles.size(); i++) {
//    particles[i].weight /= total_weight;
//    weights[i] = particles[i].weight;
//  }

  // cout << "End updateWeights \n";
}

void ParticleFilter::resample() {
	// TODO: Resample particles with replacement with probability proportional to their weight. 
	// NOTE: You may find std::discrete_distribution helpful here.
	//   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution
  // cout << "Start resample \n";
  default_random_engine gen;
  vector<Particle> sampled_particles;

  uniform_int_distribution<int> particle_index_distribution(0, num_particles - 1);
  int next_particle_id = particle_index_distribution(gen);
  double beta = 0.0;
  double max_weight = *max_element(weights.begin(), weights.end());

  uniform_real_distribution<double> random_weight_distribution(0.0, 2*max_weight);

  for (int i = 0; i < num_particles; i++) {
    beta += random_weight_distribution(gen);
    while (beta > weights[next_particle_id]) {
      beta -= weights[next_particle_id];
      next_particle_id = (next_particle_id + 1) % num_particles;
    }
    sampled_particles.push_back(particles[next_particle_id]);
  }
  particles = sampled_particles;
  // cout << "End resample \n";

}

Particle ParticleFilter::SetAssociations(Particle& particle, const std::vector<int>& associations, 
                                     const std::vector<double>& sense_x, const std::vector<double>& sense_y)
{
    //particle: the particle to assign each listed association, and association's (x,y) world coordinates mapping to
    // associations: The landmark id that goes along with each listed association
    // sense_x: the associations x mapping already converted to world coordinates
    // sense_y: the associations y mapping already converted to world coordinates

    particle.associations= associations;
    particle.sense_x = sense_x;
    particle.sense_y = sense_y;

    return particle;
}

string ParticleFilter::getAssociations(Particle best)
{
	vector<int> v = best.associations;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<int>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseX(Particle best)
{
	vector<double> v = best.sense_x;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseY(Particle best)
{
	vector<double> v = best.sense_y;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
