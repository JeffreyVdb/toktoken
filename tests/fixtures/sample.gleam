import gleam/io
import gleam/string

pub fn main() {
  io.println("Hello!")
}

pub type User {
  User(name: String, age: Int)
}

pub const max_retries = 5

fn helper(x) {
  x + 1
}
