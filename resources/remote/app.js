'use strict';
const $ = id => document.getElementById(id);
const spanish = navigator.language.toLowerCase().startsWith('es');
const words = spanish ? {heading:'Control de Switch',pair:'Código de vinculación de los ajustes de Switch',connect:'Conectar',error:'No se pudo conectar. Comprueba el código y la conexión Wi-Fi.',sent:'Enviado al Switch',busy:'Switch ocupado…',search:'Buscar pista',disconnect:'Desconectar'} : {heading:'Switch remote',pair:'Pairing code from Switch settings',connect:'Connect',error:'Cannot connect. Check the pairing code and Wi-Fi.',sent:'Sent to Switch',busy:'Switch is busy…',search:'Find a song',disconnect:'Disconnect'};
$('heading').textContent=words.heading;$('pair-label').textContent=words.pair;$('connect').textContent=words.connect;$('search-label').textContent=words.search;$('disconnect').textContent=words.disconnect;
let token=localStorage.getItem('nx-remote-code')||'',lastSongs='',polling=false;
if(/^#[0-9a-f]{32}$/i.test(location.hash)){token=location.hash.slice(1).toLowerCase();history.replaceState(null,'',location.pathname);}
async function request(path,body){
 const response=await fetch(path,{method:body?'POST':'GET',headers:{Authorization:`Bearer ${token}`,...(body?{'Content-Type':'application/json'}:{})},body:body?JSON.stringify(body):undefined,cache:'no-store'});
 if(response.status===401){localStorage.removeItem('nx-remote-code');token='';$('pair').hidden=false;$('remote').hidden=true;throw Error(words.error);}
 if(!response.ok)throw Error(words.error);
 return response.json();
}
async function command(action,value){
 try{await request('/api/command',{action,...(action==='search'?{query:value}:action==='play'?{id:value}:{})});$('message').textContent=words.sent;}
 catch(error){$('message').textContent=error.message;}
}
async function update(){
 if(!token||polling)return;polling=true;
 try{
  const state=await request('/api/state');localStorage.setItem('nx-remote-code',token);$('pair').hidden=true;$('remote').hidden=false;
  $('title').textContent=state.current?.title||state.labels?.choose_music||'';$('artist').textContent=state.current?.artist||'';$('time').textContent=state.time||'';$('view').textContent=state.view||'';
  $('play-state').textContent=state.playing?(state.paused?state.labels?.paused:state.labels?.playing):state.labels?.stopped||'';
  for(const [id,key] of Object.entries({previous:'previous',next:'next',stop:'stop',find:'search',favorites:'favorite_songs',shuffle:'shuffle_all'})){if(state.labels?.[key])$(id).textContent=state.labels[key];}
  $('toggle').textContent=state.labels?.[state.playing&&!state.paused?'pause':'play']||'Play / Pause';
  document.querySelectorAll('[data-action]').forEach(button=>button.disabled=!!state.busy);$('find').disabled=!!state.busy;
  if(state.notice)$('message').textContent=state.notice;else if(state.busy)$('message').textContent=words.busy;else if($('message').textContent===words.busy)$('message').textContent='';
  const songs=JSON.stringify(state.songs||[]);
  if(songs!==lastSongs){lastSongs=songs;$('songs').replaceChildren();for(const song of state.songs||[]){const li=document.createElement('li'),button=document.createElement('button'),title=document.createElement('span'),artist=document.createElement('small');title.textContent=song.title;artist.textContent=song.artist;button.append(title,artist);button.addEventListener('click',()=>command('play',song.id));li.append(button);$('songs').append(li);}}
 }catch(error){$('message').textContent=error.message;}finally{polling=false;}
}
$('pair').addEventListener('submit',event=>{event.preventDefault();const value=$('code').value.trim().toLowerCase();if(!/^[0-9a-f]{32}$/.test(value)){$('message').textContent=words.error;return;}token=value;update();});
$('search').addEventListener('submit',event=>{event.preventDefault();command('search',$('query').value);});
document.querySelectorAll('[data-action]').forEach(button=>button.addEventListener('click',()=>command(button.dataset.action)));
$('disconnect').addEventListener('click',()=>{token='';lastSongs='';localStorage.removeItem('nx-remote-code');$('remote').hidden=true;$('pair').hidden=false;$('code').value='';});
setInterval(update,1500);update();
