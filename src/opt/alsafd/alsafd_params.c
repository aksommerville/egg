#include "alsafd_internal.h"

/* Wipe hwparams.
 */
 
void alsafd_hw_params_any(struct snd_pcm_hw_params *params) {
  memset(params,0,sizeof(struct snd_pcm_hw_params));
  memset(params->masks,0xff,sizeof(params->masks));
  struct snd_interval *v=params->intervals;
  int i=SNDRV_PCM_HW_PARAM_FIRST_INTERVAL;
  for (;i<=SNDRV_PCM_HW_PARAM_LAST_INTERVAL;i++,v++) {
    v->max=UINT_MAX;
  }
  params->rmask=UINT_MAX;
}

void alsafd_hw_params_none(struct snd_pcm_hw_params *params) {
  memset(params,0,sizeof(struct snd_pcm_hw_params));
}

/* Read a field.
 */
 
int alsafd_hw_params_get_mask(const struct snd_pcm_hw_params *params,int k,int bit) {
  if (k<SNDRV_PCM_HW_PARAM_FIRST_MASK) return -1;
  if (k>SNDRV_PCM_HW_PARAM_LAST_MASK) return -1;
  if (bit<0) return -1;
  if (bit>=SNDRV_MASK_MAX) return -1;
  const struct snd_mask *mask=params->masks+k-SNDRV_PCM_HW_PARAM_FIRST_MASK;
  return (mask->bits[bit>>5]&(1<<bit&31))?1:0;
}

int alsafd_hw_params_get_interval(int *lo,int *hi,const struct snd_pcm_hw_params *params,int k) {
  if (k<SNDRV_PCM_HW_PARAM_FIRST_INTERVAL) return -1;
  if (k>SNDRV_PCM_HW_PARAM_LAST_INTERVAL) return -1;
  const struct snd_interval *interval=params->intervals+k-SNDRV_PCM_HW_PARAM_FIRST_INTERVAL;
  *lo=interval->min;
  if (interval->max>=INT_MAX) *hi=INT_MAX;
  else *hi=interval->max;
  return 0;
}

int alsafd_hw_params_assert_exact_interval(int *v,const struct snd_pcm_hw_params *params,int k) {
  if (k<SNDRV_PCM_HW_PARAM_FIRST_INTERVAL) return -1;
  if (k>SNDRV_PCM_HW_PARAM_LAST_INTERVAL) return -1;
  const struct snd_interval *interval=params->intervals+k-SNDRV_PCM_HW_PARAM_FIRST_INTERVAL;
  if (interval->min!=interval->max) return -1;
  *v=interval->min;
  return 0;
}

/* Set field.
 */

void alsafd_hw_params_set_mask(struct snd_pcm_hw_params *params,int k,int bit,int v) {
  if (k<SNDRV_PCM_HW_PARAM_FIRST_MASK) return;
  if (k>SNDRV_PCM_HW_PARAM_LAST_MASK) return;
  if (bit<0) return;
  if (bit>=SNDRV_MASK_MAX) return;
  params->rmask|=1<<k;
  struct snd_mask *mask=params->masks+k-SNDRV_PCM_HW_PARAM_FIRST_MASK;
  if (v) {
    mask->bits[bit>>5]|=1<<(bit&31);
  } else {
    mask->bits[bit>>5]&=~(1<<(bit&31));
  }
}

void alsafd_hw_params_set_interval(struct snd_pcm_hw_params *params,int k,int lo,int hi) {
  if (k<SNDRV_PCM_HW_PARAM_FIRST_INTERVAL) return;
  if (k>SNDRV_PCM_HW_PARAM_LAST_INTERVAL) return;
  params->rmask|=1<<k;
  struct snd_interval *interval=params->intervals+k-SNDRV_PCM_HW_PARAM_FIRST_INTERVAL;
  interval->min=lo;
  interval->max=hi;
  //TODO What do these bits mean?
  //interval->openmin=0; // 0|1
  //interval->openmax=1; // 0|1
  //interval->integer=1; // 0|1
  //interval->empty=0; // 0|1
}

void alsafd_hw_params_set_nearest_interval(struct snd_pcm_hw_params *params,int k,unsigned int v) {
  if (k<SNDRV_PCM_HW_PARAM_FIRST_INTERVAL) return;
  if (k>SNDRV_PCM_HW_PARAM_LAST_INTERVAL) return;
  struct snd_interval *interval=params->intervals+k-SNDRV_PCM_HW_PARAM_FIRST_INTERVAL;
  if (v<interval->min) interval->max=interval->min;
  else if (v>interval->max) interval->min=interval->max;
  else interval->min=interval->max=v;
}
