import React, { useEffect, useState } from 'react'
import CryptoJS from 'crypto-js'

const IP = 'http://192.168.1.251'

function encrypt(text) {
  return CryptoJS.SHA256(text).toString(CryptoJS.enc.Base64)
}

export default function ControlPanel() {
  const [username, setUsername] = useState('')
  const [password, setPassword] = useState('')
  const [passcode, setPasscode] = useState('')
  const [status, setStatus] = useState(null)

  const fetchStatus = async () => {
    try {
      const response = await fetch(`${IP}/api/status`)
      if (!response.ok) {
        throw new Error('Failed to fetch status');
      }
      const data = await response.json()
      setStatus(data)
    } catch (err) {
      console.error('Failed to fetch status:', err)
    }
  }

  const handleRequest = async (endpoint, includePasscode = false) => {
    try {
      const headers = {
        'username': username,
        'password': password,
      };

      if (includePasscode) {
        headers['passcode'] = passcode;
      }

      // Use fetch to send the request
      const response = await fetch(`${IP}/api/${endpoint}`, {
        method: 'POST',
        headers: {
          ...headers,
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({}), // Send an empty body if needed, adjust as necessary
      })

      if (!response.ok) {
        throw new Error(`Failed to send ${endpoint} request`)
      }

      alert(`${endpoint} request sent successfully`)
    } catch (err) {
      console.error(`Error on ${endpoint}:`, err)
      alert(`Failed to send ${endpoint} request`)
    }
  }

  useEffect(() => {
    fetchStatus()
  }, [])

  return (
    <div className="container">
      <h1>Control Panel</h1>
      <input
        type="text"
        placeholder="Username"
        value={username}
        onChange={e => setUsername(e.target.value)}
      />
      <input
        type="password"
        placeholder="Password"
        value={password}
        onChange={e => setPassword(e.target.value)}
      />
      <input
        type="text"
        placeholder="Passcode"
        value={passcode}
        onChange={e => setPasscode(e.target.value)}
      />

      <div>
        <button onClick={() => handleRequest('open')}>Open</button>
        <button onClick={() => handleRequest('close')}>Close</button>
        <button onClick={fetchStatus}>Reload</button>
        <button onClick={() => handleRequest('adduser', true)}>Add User</button>
      </div>

      {status && (
        <div className="status">
          <h2>Status</h2>
          <pre>{JSON.stringify(status, null, 2)}</pre>
        </div>
      )}
    </div>
  )
}
