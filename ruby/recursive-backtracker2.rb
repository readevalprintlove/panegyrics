# --------------------------------------------------------------------
# Recursive backtracking algorithm for maze generation. Requires that
# the entire maze be stored in memory, but is quite fast, easy to
# learn and implement, and (with a few tweaks) gives fairly good mazes.
# Can also be customized in a variety of ways.
# --------------------------------------------------------------------

# --------------------------------------------------------------------
# 1. Allow the maze to be customized via command-line parameters
# --------------------------------------------------------------------

width  = (ARGV[0] || 10).to_i
height = (ARGV[1] || width).to_i
seed   = (ARGV[2] || rand(0xFFFF_FFFF)).to_i

srand(seed)

grid = Array.new(height) { Array.new(width, 0) }

# --------------------------------------------------------------------
# 2. Set up constants to aid with describing the passage directions
# --------------------------------------------------------------------

N, S, E, W = 1, 2, 4, 8
DX         = { E => 1, W => -1, N =>  0, S => 0 }
DY         = { E => 0, W =>  0, N => -1, S => 1 }
OPPOSITE   = { E => W, W =>  E, N =>  S, S => N }

# --------------------------------------------------------------------
# 3. The recursive-backtracking algorithm itself
# --------------------------------------------------------------------

def carve_passages_from(cx, cy, grid)
  directions = [N, S, E, W].sort_by{rand}

  directions.each do |direction|
    nx, ny = cx + DX[direction], cy + DY[direction]

    if ny.between?(0, grid.length-1) && nx.between?(0, grid[ny].length-1) && grid[ny][nx] == 0
      grid[cy][cx] |= direction
      grid[ny][nx] |= OPPOSITE[direction]
      carve_passages_from(nx, ny, grid)
    end
  end
end

carve_passages_from(0, 0, grid)

# --------------------------------------------------------------------
# 4. A simple routine to emit the maze as ASCII
# --------------------------------------------------------------------

puts " " + "_" * (width * 2 - 1)
height.times do |y|
  print "|"
  width.times do |x|
    print((grid[y][x] & S != 0) ? " " : "_")
    if grid[y][x] & E != 0
      print(((grid[y][x] | grid[y][x+1]) & S != 0) ? " " : "_")
    else
      print "|"
    end
  end
  puts
end

# --------------------------------------------------------------------
# 5. Show the parameters used to build this maze, for repeatability
# --------------------------------------------------------------------

puts "#{$0} #{width} #{height} #{seed}"
