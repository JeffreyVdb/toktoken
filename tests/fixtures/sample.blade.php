@extends('layouts.app')

@section('title', 'Sample Page')

@section('content')
    <div class="container">
        <h1>{{ $title }}</h1>

        @include('partials.header')

        @component('components.alert')
            @slot('type', 'success')
            This is a sample alert.
        @endcomponent

        @yield('sidebar')

        @include('partials.footer')
    </div>
@endsection
