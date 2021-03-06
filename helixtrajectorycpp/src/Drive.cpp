#include "Drive.h"
#include "TrajectoryUtil.h"

namespace helixtrajectory {

    Drive::Drive(double mass, double moi, const Obstacle& bumpers)
        : mass(mass), moi(moi), bumpers(bumpers) {
    }

    const casadi::MX Drive::SolveBumperCornerPosition(const casadi::MX& x, const casadi::MX& y,
            const casadi::MX& theta, const ObstaclePoint& bumperCorner) const {
        casadi::MX position(2, 1);
        double cornerDiagonal = hypot(bumperCorner.x, bumperCorner.y);
        double cornerAngle = atan2(bumperCorner.y, bumperCorner.x);
        std::cout << "About to assign bumper corner position" << std::endl;
        std::cout << "Row count = " << theta.rows() << std::endl;
        std::cout << "Col count = " << theta.columns() << std::endl;
        position(0) = x + cornerDiagonal * cos(cornerAngle + theta);
        position(1) = y + cornerDiagonal * sin(cornerAngle + theta);
        std::cout << "Assigned bumper corner poses" << std::endl;
        return position;
    }

    void Drive::ApplyObstacleConstraints(casadi::Opti& opti, const casadi::MX& x, const casadi::MX& y,
            const casadi::MX& theta, size_t nTotal, const std::vector<Obstacle>& obstacles) const {
        for (size_t i = 0; i < nTotal; i++) {
            for (const Obstacle& obstacle : obstacles) {
                double distSquared = bumpers.safetyDistance + obstacle.safetyDistance;
                distSquared = distSquared * distSquared;
                size_t bumperCornerCount = bumpers.points.size();
                size_t obstacleCornerCount = obstacle.points.size();
                if (bumperCornerCount == 1 && obstacleCornerCount == 1) {
                    const ObstaclePoint& bumperCorner = bumpers.points[0];
                    const ObstaclePoint& obstaclePoint = obstacle.points[0];
                    if (bumperCorner.x == 0 && bumperCorner.y == 0) {
                        casadi::MX deltaX = obstaclePoint.x - x(i);
                        casadi::MX deltaY = obstaclePoint.y - y(i);
                        casadi::MX pointDistSquared = deltaX * deltaX + deltaY * deltaY;
                        opti.subject_to(pointDistSquared >= distSquared);
                    } else  {
                        casadi::MX bumperCornerPosition = SolveBumperCornerPosition(x(i), y(i), theta(i), bumperCorner);
                        casadi::MX deltaX = obstaclePoint.x - bumperCornerPosition(0);
                        casadi::MX deltaY = obstaclePoint.y - bumperCornerPosition(1);
                        casadi::MX pointDistSquared = deltaX * deltaX + deltaY * deltaY;
                        opti.subject_to(pointDistSquared >= distSquared);
                    }
                } else {
                    for (size_t bumperCornerIndex = 0; bumperCornerIndex < bumperCornerCount - 1; bumperCornerIndex++) {
                        std::cout << "About to solve bumper positions" << std::endl;
                        casadi::MX startBumperCornerPosition = SolveBumperCornerPosition(x(i), y(i), theta(i), bumpers.points[bumperCornerIndex]);
                        casadi::MX endBumperCornerPosition = SolveBumperCornerPosition(x(i), y(i), theta(i), bumpers.points[bumperCornerIndex + 1]);
                        std::cout << "solved bumper positions" << std::endl;
                        for (const ObstaclePoint& obstaclePoint : obstacle.points) {
                            std::cout << "Finding line point dist" << std::endl;
                            casadi::MX dist = linePointDist(startBumperCornerPosition(0), startBumperCornerPosition(1),
                                    endBumperCornerPosition(0), endBumperCornerPosition(1), obstaclePoint.x, obstaclePoint.y);
                            std::cout << "Line point dist found" << std::endl;
                            opti.subject_to(dist >= distSquared);
                            std::cout << "Constrained line point dist" << std::endl;
                        }
                    }
                    if (bumperCornerCount >= 3) { // must have at least three points to make a closed shape
                        casadi::MX startBumperCornerPosition = SolveBumperCornerPosition(x(i), y(i), theta(i), bumpers.points[bumperCornerCount - 1]);
                        casadi::MX endBumperCornerPosition = SolveBumperCornerPosition(x(i), y(i), theta(i), bumpers.points[0]);
                        for (const ObstaclePoint& obstaclePoint : obstacle.points) {
                            casadi::MX dist = linePointDist(startBumperCornerPosition(0), startBumperCornerPosition(1),
                                    endBumperCornerPosition(0), endBumperCornerPosition(1), obstaclePoint.x, obstaclePoint.y);
                            opti.subject_to(dist >= distSquared);
                        }
                    }

                    for (size_t obstacleCornerIndex = 0; obstacleCornerIndex < obstacleCornerCount - 1; obstacleCornerIndex++) {
                        double startObstacleCornerX = obstacle.points[obstacleCornerIndex].x;
                        double startObstacleCornerY = obstacle.points[obstacleCornerIndex].y;
                        double endObstacleCornerX = obstacle.points[obstacleCornerIndex + 1].x;
                        double endObstacleCornerY = obstacle.points[obstacleCornerIndex + 1].y;
                        for (const ObstaclePoint& bumperCorner : bumpers.points) {
                            casadi::MX bumperCornerPosition = SolveBumperCornerPosition(x(i), y(i), theta(i), bumperCorner);
                            casadi::MX dist = linePointDist(startObstacleCornerX, startObstacleCornerY,
                                    endObstacleCornerX, endObstacleCornerY, bumperCornerPosition(0), bumperCornerPosition(1));
                            opti.subject_to(dist >= bumpers.safetyDistance + obstacle.safetyDistance);
                        }
                    }
                    if (obstacleCornerCount >= 3) {
                        double startObstacleCornerX = obstacle.points[obstacleCornerCount - 1].x;
                        double startObstacleCornerY = obstacle.points[obstacleCornerCount - 1].y;
                        double endObstacleCornerX = obstacle.points[0].x;
                        double endObstacleCornerY = obstacle.points[0].y;
                        for (const ObstaclePoint& bumperCorner : bumpers.points) {
                            casadi::MX bumperCornerPosition = SolveBumperCornerPosition(x(i), y(i), theta(i), bumperCorner);
                            casadi::MX dist = linePointDist(startObstacleCornerX, startObstacleCornerY,
                                    endObstacleCornerX, endObstacleCornerY, bumperCornerPosition(0), bumperCornerPosition(1));
                            opti.subject_to(dist >= bumpers.safetyDistance + obstacle.safetyDistance);
                        }
                    }
                }
            }
        }
    }
}