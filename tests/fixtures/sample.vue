<template>
  <div>{{ message }}</div>
</template>

<script setup>
import { ref, computed } from 'vue'

const props = defineProps<{
  title: string
}>()

const message = ref('Hello')
const count = ref(0)
const doubled = computed(() => count.value * 2)

function increment() {
  count.value++
}

const fetchData = async () => {
  return await fetch('/api')
}
</script>
