module MyModule

abstract type AbstractShape end

struct Point
    x::Float64
    y::Float64
end

mutable struct Circle
    center::Point
    radius::Float64
end

function area(c::Circle)
    return pi * c.radius^2
end

function perimeter(c::Circle)
    return 2 * pi * c.radius
end

macro assert_positive(expr)
    quote
        val = $(esc(expr))
        val > 0 || error("Expected positive value")
    end
end

const MAX_RADIUS = 1000.0
const MIN_RADIUS = 0.1

end # module
