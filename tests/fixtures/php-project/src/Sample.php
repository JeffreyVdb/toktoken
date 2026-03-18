<?php

declare(strict_types=1);

namespace App;

/**
 * Sample class for testing ctags parsing.
 *
 * This docstring has multiple lines,
 * including @param and @return tags that should be stripped.
 */
class Sample implements SampleInterface
{
    use LoggableTrait;

    public const MAX_RETRIES = 3;

    private string $name;

    protected int $count = 0;

    /**
     * Create a new Sample instance.
     */
    public function __construct(string $name)
    {
        $this->name = $name;
    }

    /**
     * Get the name.
     */
    public function getName(): string
    {
        return $this->name;
    }

    /**
     * Process items with retry logic.
     *
     * @param array $items Items to process.
     * @return int Number of processed items.
     */
    public function process(array $items): int
    {
        $processed = 0;
        foreach ($items as $item) {
            if ($this->validate($item)) {
                $processed++;
            }
        }
        $this->count += $processed;
        return $processed;
    }

    private function validate(mixed $item): bool
    {
        return $item !== null;
    }

    /**
     * Method with braces inside strings — stress test for estimateEndLine.
     */
    public function braceInStrings(): string
    {
        $a = "this has { braces } inside";
        $b = 'also { here } in single quotes';
        $c = "nested \"quote with { brace }\"";
        if (true) {
            return $a . $b . $c;
        }
        return '';
    }

    /**
     * Method with braces inside comments.
     */
    public function braceInComments(): int
    {
        // This line has { an opening brace in a comment
        $x = 1;
        /* This block comment has { braces } too */
        $y = 2;
        return $x + $y; // and another { here
    }

    /**
     * Deeply nested method to stress brace counting.
     */
    public function deeplyNested(array $data): array
    {
        $result = [];
        foreach ($data as $key => $value) {
            if (is_array($value)) {
                foreach ($value as $k => $v) {
                    if ($v !== null) {
                        $result[$key][$k] = $v;
                    }
                }
            } else {
                $result[$key] = $value;
            }
        }
        return $result;
    }
}

interface SampleInterface
{
    public function getName(): string;

    public function process(array $items): int;
}

trait LoggableTrait
{
    public function log(string $message): void
    {
        // no-op for testing
    }
}

enum Status: string
{
    case Active = 'active';
    case Inactive = 'inactive';
}

/**
 * Helper function for testing.
 */
function sampleHelper(int $x): int
{
    return $x * 2;
}

define('SAMPLE_VERSION', '1.0.0');
