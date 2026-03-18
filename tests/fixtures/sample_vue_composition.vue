<template>
  <div>{{ message }}</div>
</template>

<script setup>
import { ref, computed, watch, watchEffect, onMounted, onUnmounted, provide, inject } from 'vue'

const props = defineProps<{
  title: string
}>()

const emit = defineEmits(['update', 'close'])

defineExpose({
  reset,
  validate
})

const message = ref('Hello')
const count = ref(0)
const doubled = computed(() => count.value * 2)

watch(count, (newVal) => {
  console.log('count changed:', newVal)
})

watchEffect(() => {
  document.title = message.value
})

onMounted(() => {
  console.log('mounted')
})

onUnmounted(() => {
  console.log('unmounted')
})

provide('theme', 'dark')

const locale = inject('locale')

function increment() {
  count.value++
}

const fetchData = async (url) => {
  return await fetch(url)
}
</script>
