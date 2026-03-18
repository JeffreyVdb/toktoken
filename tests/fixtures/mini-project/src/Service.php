<?php

declare(strict_types=1);

namespace Test;

/**
 * Service class for E2E update and outline tests.
 */
class Service
{
    private App $app;

    public function __construct(App $app)
    {
        $this->app = $app;
    }

    /**
     * Execute the main service logic.
     */
    public function execute(): bool
    {
        return $this->app->run() === 0;
    }

    public function getApp(): App
    {
        return $this->app;
    }
}
