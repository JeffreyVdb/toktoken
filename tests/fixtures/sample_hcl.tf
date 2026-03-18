resource "aws_instance" "web" {
  ami           = "ami-12345678"
  instance_type = "t2.micro"
}

variable "region" {
  description = "AWS region"
  default     = "us-east-1"
}

module "vpc" {
  source = "./modules/vpc"
}

output "instance_ip" {
  value = aws_instance.web.public_ip
}

provider "aws" {
  region = var.region
}

data "aws_ami" "ubuntu" {
  most_recent = true
}

locals {
  common_tags = {
    Environment = "dev"
  }
}

terraform {
  required_version = ">= 1.0"
}
