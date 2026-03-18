<?php

declare(strict_types=1);

namespace App;

/**
 * Class with edge cases for estimateEndLine and docstring extraction.
 */
class Nested
{
    /**
     * Block comment with braces: { } and asterisks *** inside.
     */
    public function blockCommentBraces(): void
    {
        /*
         * Inside a block comment: { opening
         * and } closing braces should be ignored
         */
        $x = 1;
    }

    // Single-line comment docstring
    public function lineCommentDoc(): int
    {
        return 42;
    }

    # Hash-style comment docstring
    public function hashCommentDoc(): int
    {
        return 99;
    }

    public function escapedQuotes(): string
    {
        $a = "escaped \" quote with { brace";
        $b = 'escaped \' quote with { brace';
        return $a . $b;
    }

    public function mixedCommentsAndStrings(): array
    {
        $data = [];
        // comment with { brace
        $data[] = "string with } brace";
        /* block { comment } */
        $data[] = 'another { string }';
        return $data;
    }

    public function emptyMethod(): void
    {
    }
}
