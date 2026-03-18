<?php

declare(strict_types=1);

namespace Test;

/**
 * Minimal application class for E2E testing.
 */
class App
{
    private string $version = '1.0.0';

    public function getVersion(): string
    {
        return $this->version;
    }

    public function run(): int
    {
        return 0;
    }

    /**
     * Search for items by keyword.
     */
    public function search(string $keyword): array
    {
        return [];
    }
}

function helper(): string
{
    return 'ok';
}
