import matplotlib.pyplot as plt
import math


def to_rad(angle=0.0):
    return angle * math.pi / 180.0


def plot_trajectory(positions, title="trajecory_", iteration=-1):
    if iteration != -1:
        title = f"{title}{iteration:04d}"

    #print(positions)
    x = [position.x for position in positions]
    #print(x)
    y = [position.y for position in positions]
    #print(y)
    ymax = max(y)

    screen_x = [ball.get_inputs().d_screen, ball.get_inputs().d_screen]
    screen_y = [0.0, ball.get_inputs().h_screen]
    wall_x   = [ball.get_inputs().d_wall, ball.get_inputs().d_wall]
    wall_y   = [0.0, ball.get_inputs().h_wall]
    x_bound = wall_x[1] + 2.0

    ymax = max(ymax, wall_y[1])

    plt.plot(x,y)
    plt.plot(screen_x, screen_y)
    plt.plot(wall_x, wall_y)
    plt.xlim(0, x_bound)
    plt.ylim(0, ymax + 2.0)
    plt.xlabel("distance")
    plt.ylabel("height")
    plt.title(title)
    plt.savefig(f"{title}.png")
    plt.close()


class Pair:
    def __init__(self, x=0.0, y=0.0):
        self.x = x
        self.y = y

    def __str__(self):
        return (f"x={self.x},y={self.y}")

    def __add__(self, pair_addend):
        return Pair(self.x + pair_addend.x, self.y + pair_addend.y)

    def scale(self, scalar):
        return Pair(self.x * scalar, self.y * scalar)


class PairVector:

    def __init__(self, pairs_vector=None):
        # Empty array if no vector else set the vectors equal
        self.pairs = pairs_vector if pairs_vector is not None else []

    def append(self, new):
        self.pairs.append(new)

    # Iter method to plot pairs
    def __iter__(self):
        for pair in self.pairs:
            yield pair


class Inputs:

    # Initialize fields to zero by default
    def __init__(self, path_to_inputfile):
        self.__file_path = path_to_inputfile
        self.mass     = 0.0
        self.drag     = 0.0
        self.velo     = 0.0
        self.d_screen = 0.0
        self.h_screen = 0.0
        self.d_target = 0.0
        self.d_wall   = 0.0
        self.h_wall   = 0.0
        self.w_velo   = 0.0
        self.dt       = 0.0
        self.tol      = 0.0

    # Test __str__ method to visualize read data
    def __str__(self):
        return (f"Inputs(mass={self.mass}, drag={self.drag}, velo={self.velo}, "
            f"d_screen={self.d_screen}, h_screen={self.h_screen}, d_target={self.d_target}, "
            f"d_wall={self.d_wall}, h_wall={self.h_wall}, w_velo={self.w_velo}, dt={self.dt}, "
            f"tol={self.tol})")

    # Method to extract inputs from datafile 
    def parse_inputs(self):
        with open(self.__file_path, "r") as f:
            lines = [line.strip() for line in f]

        self.mass     = float(lines[0])
        self.drag     = float(lines[1])
        self.init_vel = float(lines[2]) 
        self.d_screen = float(lines[3])
        self.h_screen = float(lines[4])
        self.d_target = float(lines[5])
        self.d_wall   = float(lines[6])
        self.h_wall   = float(lines[7])
        self.w_velo   = float(lines[8])
        self.dt       = float(lines[9])
        self.tol      = float(lines[10])

        return self

    # Set a different inputfile to rerun program with new parameters 
    def set_inputfile(self, new_filepath):
        self.__file_path = new_filepath


class Ball:
    
    def __init__(self, path_to_inputfile):
        self.inputs = Inputs(path_to_inputfile).parse_inputs()
        self.velo   = Pair() # v_x and v_z
        self.position = Pair()

    # Takes the interpolation to estimate distance at y=0.0
    def linear_interpolate(self, next, y=0.0):
        ratio = (y - self.position.y) / (next.y - self.position.y) 
        interpolated_x = self.position.x + ratio * (next.x - self.position.x)  
        return Pair(interpolated_x, 0.0)
    
    # State is a vector of Pairs
    def acceleration(self, velo):
        vxe = velo.x - self.inputs.w_velo
        ve  = math.sqrt(vxe**2 + velo.y**2)
        ax = -self.inputs.drag * vxe * ve;
        ay = -self.inputs.mass * 9.81 - self.inputs.drag * velo.y * ve
        return Pair(ax, ay).scale(1.0 / self.inputs.mass)

    # Check if intersecting with a wall or screen 
    def check_intersection(self, next, check_values):
        # Check if position is to left and next is to left they are equal. 1 != 1 false proceed? 
        if (self.position.x - check_values.x) * (next.x - check_values.x) > 0:
            return False, next

        if self.position.x - check_values.x < 0:
            direction = -1.0 
        else:
            direction = 1.0

        tol = direction * self.inputs.tol 
        adjusted_x = check_values.x + tol

        ratio = (adjusted_x - self.position.x) / (next.x - self.position.x)
        interpolated_y = self.position.y + ratio * (next.y - self.position.y)

        if interpolated_y <= check_values.y + self.inputs.tol:
            return True, Pair(adjusted_x, interpolated_y)

        return False, next
    
    # Simulates the motion of a shot from a specified angle
    @staticmethod
    def shot_rk4(ball, angle, plot=False):

        ball.position  = Pair() 
        ball.velo.x = ball.inputs.init_vel * math.cos(angle)
        ball.velo.y = ball.inputs.init_vel * math.sin(angle)

        if plot:
            # Create vector to store position 
            positions = PairVector()

        # While motion is being simulated for shot 
        simulating = True
        while simulating:

            # Weighted sum for both position and velocity update
            k1_velocity = ball.velo 
            k1_accel    = Ball.acceleration(ball, k1_velocity)

            k2_velocity = ball.velo + k1_accel.scale(0.5 * ball.inputs.dt)
            k2_accel    = Ball.acceleration(ball, k2_velocity)

            k3_velocity = ball.velo + k2_accel.scale(0.5 * ball.inputs.dt)
            k3_accel    = Ball.acceleration(ball, k3_velocity)

            k4_velocity = ball.velo + k3_accel.scale(ball.inputs.dt)
            k4_accel    = Ball.acceleration(ball, k4_velocity)

            # Weighted Sum calculation from K weights 
            velocity_sum = (k1_velocity + k2_velocity.scale(2.0) + k3_velocity.scale(2.0) + k4_velocity).scale(1.0/6.0)
            accel_sum    = (k1_accel + k2_accel.scale(2.0) + k3_accel.scale(2.0) + k4_accel).scale(1.0/6.0)

            # Set next values
            next         = ball.position + velocity_sum.scale(ball.inputs.dt)
            ball.velo    = ball.velo + accel_sum.scale(ball.inputs.dt)

            # print("Suppress")
            # Check if ball is intersecting with wall or screen
            hit_wall, next = Ball.check_intersection(ball, next, Pair(ball.inputs.d_screen, ball.inputs.h_screen))
            if hit_wall:
                ball.velo.x *= -1.0

            hit_wall, next = Ball.check_intersection(ball, next, Pair(ball.inputs.d_wall, ball.inputs.h_wall))
            if hit_wall:
                ball.velo.x *= -1.0

            # Check if interpolated with floor 
            if next.y <= ball.inputs.tol and ball.velo.y < 0.0:
                next = Ball.linear_interpolate(ball, next, 0.0)
                simulating = False
            else:
                ball.position = next
                # print(next)

            if plot:
                positions.append(ball.position)
        if plot:
            return next, positions 
        else:
            return next, []

    @staticmethod 
    def shot_eul(ball, angle, plot=False):

        ball.position  = Pair() 
        ball.velo.x = ball.inputs.init_vel * math.cos(angle)
        ball.velo.y = ball.inputs.init_vel * math.sin(angle)

        if plot:
            # Create vector to store position 
            positions = PairVector()

        # While motion is being simulated for shot 
        simulating = True
        while simulating:

            next = ball.position + ball.velo.scale(ball.inputs.dt)
            ball.velo += Ball.acceleration(ball, ball.velo).scale(ball.inputs.dt)

            hit_wall, next = Ball.check_intersection(ball, next, Pair(ball.inputs.d_screen, ball.inputs.h_screen))
            if hit_wall:
                ball.velo.x *= -1

            hit_wall, next = Ball.check_intersection(ball, next, Pair(ball.inputs.d_wall, ball.inputs.h_wall))
            if hit_wall:
                ball.velo.x *= -1

            # Check if interpolated with floor 
            if next.y < ball.inputs.tol and ball.velo.y < 0.0:
                next = Ball.linear_interpolate(ball, next, 0.0)
                simulating = False
            else:
                ball.position = next
                #print(next)

            if plot:
                positions.append(ball.position)
        if plot:
            return next, positions 
        else:
            return next, []
    


    def get_inputs(self):
        return self.inputs


class Trajectory:


    def __init__(self, ball, shot=Ball.shot_eul):
        self.__ball = ball
        self.__bounds = Pair(to_rad(0.0), to_rad(30.0))
        # Find valid bounds for a unique ball object
        lower, _ = shot(ball, self.__bounds.x, False)
        lb = lower.x 
        upper, _ = shot(ball, self.__bounds.y, False)
        self.__ub = upper.x

        # Fetch target
        d_target = ball.get_inputs().d_target

        # Constrain the upper bound till solution is bounded by initial values 
        while (lb - d_target) * (self.__ub - d_target) > 0:
            self.__bounds.y += to_rad(1.0)
            upper, _ = shot(ball, self.__bounds.y, False)
            self.__ub = upper.x
            #print(self.__ub)

        
    def find_trajectory_bisect(self, plot=False, shot=Ball.shot_eul):
        i = 0
        # Collect constant parameters locally 
        d_target = self.__ball.get_inputs().d_target
        tol      = self.__ball.get_inputs().tol

        # Bisection to collect optimal angle
        error = [self.__ub - d_target] 
        trajectory_found = False
        while trajectory_found == False and i < 100:
            midpoint = (self.__bounds.x + self.__bounds.y) / 2.0
            pos, positions = shot(self.__ball, midpoint, plot)
            error.append(pos.x - d_target)

            # Check for completion or how to restrict bounds 
            if abs(pos.x - d_target) < tol:
                trajectory_found = True
            elif pos.x > d_target + tol:
                self.__bounds.y = midpoint 
            else:
                self.__bounds.x = midpoint 

            i += 1
            print(f"Iteration {i}, Error: {error[i]}")
            plot_trajectory(positions, iteration=i)
        
        return midpoint, error

    def get_bounds(self):
        return self.__bounds

ball = Ball("inputs1-3.txt")
#print(ball.get_inputs())
final, _ = Ball.shot_rk4(ball, to_rad(45), plot=True)
#print(final)

bisector = Trajectory(ball, Ball.shot_rk4)
print(f"Bounded Problem: {bisector.get_bounds()}")

optimal_angle, error = bisector.find_trajectory_bisect(True, Ball.shot_rk4)
_, positions = Ball.shot_rk4(ball, optimal_angle, plot=True)
plot_trajectory(positions, "optimal_angle_rk4")

optimal_angle, error = bisector.find_trajectory_bisect(True, Ball.shot_eul)
_, positions = Ball.shot_eul(ball, optimal_angle, plot=True)
plot_trajectory(positions, "optimal_angle_eul")

# Create vector to plot based on trajectory 

