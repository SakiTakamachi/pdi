<?php

/** @generate-class-entries */

class Pdi
{
	//public static function container(string $name): Pdi {}
	public function __construct() {}

	public function bind(string $abstract, callable|string $concrete): void {}

	public function singleton(string $abstract, callable|string $concrete): void {}

	public function make(string $abstract, array $parameters = []): object {}

	//public function swap(string $abstract, object $instance): void {}

	//public function clearSwap(): void {}
}

class PdiException extends RuntimeException
{
}
